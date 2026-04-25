//! End-to-end smoke test ([F7] in `docs/rust_migration.md` §5).
//!
//! Exercises the world create → mutate → save → drop → reopen → verify flow
//! using only public crate APIs. The wgpu / window layer is not involved —
//! the goal is to validate that the simulation + persistence layer survives
//! a process-style restart. Uses `World::new_at` with an absolute scratch
//! path so the test doesn't depend on cwd.

mod common;

use std::sync::Arc;

use neworld::blocks::{BlockRegistry, register_base_blocks};
use neworld::commands::{CommandRegistry, register_base_commands};
use neworld::math::Vec3i;
use neworld::worlds::World;

use common::ScratchDir;

/// Pump async chunk loading until `target_count` chunks are in the slab, or
/// 256 idle iterations have elapsed. The async pipeline is single-threaded
/// behind a worker, so a small sleep between polls keeps the test bounded.
fn pump_until_loaded(world: &mut World, target_count: usize) {
    let mut idle = 0u32;
    let mut last_count = 0usize;
    while world.chunks().len() < target_count && idle < 256 {
        world.tick_chunk_loading_async();
        let _ = world.poll_load_results();
        let now = world.chunks().len();
        if now == last_count {
            idle += 1;
            std::thread::sleep(std::time::Duration::from_millis(2));
        } else {
            idle = 0;
            last_count = now;
        }
    }
}

#[test]
fn round_trip_world_through_set_block_save_reopen() {
    let scratch = ScratchDir::new("rt");

    let world_name = "smoke-rt".to_owned();
    let render_distance = 1;
    let seed = 0x00C0_FFEE;
    let target_chunks = ((2 * render_distance + 1) as usize).pow(3);

    let coord = Vec3i::new(3, 5, 7);
    let stone_id;

    {
        let mut registry = BlockRegistry::new();
        let base = register_base_blocks(&mut registry);
        stone_id = base.stone;
        let registry = Arc::new(registry);

        let mut world = World::new_at(
            scratch.path(),
            world_name.clone(),
            render_distance,
            seed,
            Arc::clone(&registry),
            base,
        )
        .expect("World::new_at (initial)");
        world.set_center(Vec3i::new(0, 0, 0));
        pump_until_loaded(&mut world, target_chunks);
        assert_eq!(
            world.chunks().len(),
            target_chunks,
            "expected all chunks to load via async pipeline"
        );

        // Verify pre-mutation state — should not already be stone.
        let before = world.block(coord).expect("block loaded").id;
        assert_ne!(
            before, stone_id,
            "test coord must not already be stone in generated terrain"
        );

        world.set_block(coord, stone_id, false);
        assert_eq!(
            world.block(coord).expect("block still loaded").id,
            stone_id,
            "set_block did not take"
        );

        world.save_to_disk().expect("save_to_disk");
        // Drop here — the World's pipeline worker shuts down via Drop.
    }

    // Reopen the same world; the chunk that was modified must come back from
    // sled with its mutation intact (rather than being re-generated).
    {
        let mut registry = BlockRegistry::new();
        let base = register_base_blocks(&mut registry);
        let registry = Arc::new(registry);

        let mut world = World::new_at(
            scratch.path(),
            world_name.clone(),
            render_distance,
            seed,
            Arc::clone(&registry),
            base,
        )
        .expect("World::new_at (reopen)");
        world.set_center(Vec3i::new(0, 0, 0));
        pump_until_loaded(&mut world, target_chunks);

        let after = world.block(coord).expect("block loaded after reopen").id;
        assert_eq!(
            after, stone_id,
            "set_block did not survive save/reopen round trip"
        );
    }
}

#[test]
fn slash_command_dispatch_through_full_stack() {
    let scratch = ScratchDir::new("cmd");

    let mut registry = BlockRegistry::new();
    let base = register_base_blocks(&mut registry);
    let registry = Arc::new(registry);

    let mut world = World::new_at(
        scratch.path(),
        "smoke-cmd".to_owned(),
        1,
        0xDEAD_BEEF,
        Arc::clone(&registry),
        base,
    )
    .expect("World::new_at");
    world.set_center(Vec3i::new(0, 0, 0));
    pump_until_loaded(&mut world, 27);

    let mut commands = CommandRegistry::new();
    register_base_commands(&mut commands, &base, Arc::clone(&registry));

    // `/setblock <x> <y> <z> <id>` — exercises argument parsing, world
    // mutation, and the registry dispatch path. The C++ command takes a
    // numeric block id (matching the Rust port).
    let stone_id_int: u16 = base.stone.0;
    let line = format!("/setblock 4 6 8 {stone_id_int}");
    let mut messages = Vec::<String>::new();
    let ok = commands.execute_on(&line, &mut world, &mut messages);
    assert!(ok, "/setblock should succeed: {messages:?}");
    assert_eq!(world.block(Vec3i::new(4, 6, 8)).expect("loaded").id, base.stone);

    // `/help` — read-only, should always succeed and push at least one line.
    messages.clear();
    let ok = commands.execute_on("/help", &mut world, &mut messages);
    assert!(ok, "/help should succeed: {messages:?}");
    assert!(!messages.is_empty(), "/help should produce output");
}
