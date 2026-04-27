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
    while world.loaded_count() < target_count && idle < 256 {
        world.tick_chunk_loading_async();
        let _ = world.poll_load_results();
        let now = world.loaded_count();
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
    // World loads `render_distance + LOAD_RADIUS_BUFFER` (= 2 here) chunks
    // out from the centre so every meshable chunk has all 6 axis neighbours
    // loaded — see `worlds::world::LOAD_RADIUS_BUFFER`.
    let load_radius = render_distance + 1;
    let target_chunks = ((2 * load_radius + 1) as usize).pow(3);

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
            world.loaded_count(),
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

#[test]
fn lcm3_commands_drive_world_state_and_reset_connected_component() {
    use neworld::blocks::{OrientationCodec, State};

    let scratch = ScratchDir::new("lcm3");

    let mut registry = BlockRegistry::new();
    let base = register_base_blocks(&mut registry);
    let registry = Arc::new(registry);

    let mut world = World::new_at(
        scratch.path(),
        "smoke-lcm3".to_owned(),
        1,
        0xCAFE_F00D,
        Arc::clone(&registry),
        base,
    )
    .expect("World::new_at");
    world.set_center(Vec3i::new(0, 0, 0));
    pump_until_loaded(&mut world, 27);

    let mut commands = CommandRegistry::new();
    register_base_commands(&mut commands, &base, Arc::clone(&registry));

    // /lcm3-clock-rate: round-trip through the world field.
    assert_eq!(world.lcm3_clock_rate(), 1, "default clock rate");
    let mut messages = Vec::<String>::new();
    assert!(commands.execute_on("/lcm3-clock-rate 5", &mut world, &mut messages));
    assert_eq!(world.lcm3_clock_rate(), 5);
    assert!(messages.iter().any(|m| m.contains("set to 5")));
    // Bad arg → `false` return + "Fail to execute" message.
    messages.clear();
    assert!(!commands.execute_on("/lcm3-clock-rate xyz", &mut world, &mut messages));

    // Lay down a connected blob of LCM3 blocks: a wire at (1,1,1), a fork
    // at (2,1,1) (face-shared), and a non-LCM3 stone at (3,1,1) that
    // terminates BFS propagation. Place an isolated wire at (5,5,5)
    // that should NOT be reset (different connected component).
    let coord_wire = Vec3i::new(1, 1, 1);
    let coord_fork = Vec3i::new(2, 1, 1);
    let coord_stone = Vec3i::new(3, 1, 1);
    let coord_isolated = Vec3i::new(5, 5, 5);

    // Write wire / fork with non-zero data + clock interior so the
    // reset has something to clear. State `4 = (0*2 + 1) * 3 + 1` →
    // orientation 0 (Y-axis), data 1, clock 1.
    let dirty = State(4);
    world.set_block_with_state(coord_wire, base.lcm3_wire, dirty, true);
    world.set_block_with_state(coord_fork, base.lcm3_fork, dirty, true);
    world.set_block(coord_stone, base.stone, true);
    world.set_block_with_state(coord_isolated, base.lcm3_wire, dirty, true);

    // Sanity: dirty interiors stick.
    assert_eq!(world.block(coord_wire).expect("loaded").state, dirty);
    assert_eq!(world.block(coord_fork).expect("loaded").state, dirty);
    assert_eq!(world.block(coord_isolated).expect("loaded").state, dirty);

    // /lcm3-reset on the wire — should reset wire + fork (one connected
    // component, blocked by stone), leave isolated wire alone.
    messages.clear();
    let line = format!(
        "/lcm3-reset {} {} {}",
        coord_wire.x, coord_wire.y, coord_wire.z
    );
    assert!(commands.execute_on(&line, &mut world, &mut messages));
    let expected_base = OrientationCodec::LCM3.reset_to_base(dirty);
    assert_eq!(world.block(coord_wire).expect("loaded").state, expected_base);
    assert_eq!(world.block(coord_fork).expect("loaded").state, expected_base);
    assert_eq!(
        world.block(coord_isolated).expect("loaded").state,
        dirty,
        "isolated component must not be reset"
    );
    assert_eq!(
        world.block(coord_stone).expect("loaded").id,
        base.stone,
        "non-LCM3 block left intact"
    );
    assert!(messages.iter().any(|m| m.contains("reset 2 LCM3 blocks")));

    // /lcm3-reset on a non-LCM3 block — should fail-soft with a message
    // and not crash.
    messages.clear();
    assert!(commands.execute_on("/lcm3-reset 3 1 1", &mut world, &mut messages));
    assert!(messages.iter().any(|m| m.contains("not an LCM3 block")));
}

/// Pack `(orientation_index, data, clock)` into an LCM3 state byte —
/// `(orientation*2 + data)*3 + clock`. Mirrors
/// `OrientationCodec::LCM3.write` semantics; kept as a local helper so
/// the rule-application tests below read like the spec in
/// `docs/block_updates.md`.
fn lcm3_state(orientation: u8, data: u8, clock: u8) -> neworld::blocks::State {
    neworld::blocks::State((orientation % 8) * 6 + (data & 1) * 3 + (clock % 3))
}

fn lcm3_clock(s: neworld::blocks::State) -> u8 {
    s.0 % 3
}

fn lcm3_data(s: neworld::blocks::State) -> u8 {
    (s.0 / 3) & 1
}

fn lcm3_orientation(s: neworld::blocks::State) -> u8 {
    s.0 / 6
}

/// Stand up a small world for the LCM3 update tests. Returns the world
/// with chunks pre-loaded around the origin.
fn lcm3_world(scratch_tag: &str) -> (ScratchDir, World, neworld::blocks::BaseBlocks) {
    let scratch = ScratchDir::new(scratch_tag);
    let mut registry = BlockRegistry::new();
    let base = register_base_blocks(&mut registry);
    let registry = Arc::new(registry);
    let mut world = World::new_at(
        scratch.path(),
        format!("smoke-lcm3-{scratch_tag}"),
        1,
        0xFEED_F00D,
        Arc::clone(&registry),
        base,
    )
    .expect("World::new_at");
    world.set_center(Vec3i::new(0, 0, 0));
    pump_until_loaded(&mut world, 27);
    (scratch, world, base)
}

#[test]
fn placing_lcm3_block_triggers_rule_on_placed_cell() {
    // The block-update queue normally only fires the *neighbours* of a
    // freshly-placed cell — but for LCM3 blocks the rule must run on
    // the placed cell itself, since its inputs may already be in the
    // right state.
    //
    // Set up: a wire (cap +Y, data 1, clock 0) sits at (0, 0, 0). Then
    // we *place* an FF (cap +Y, data 0, clock 0) at (0, 1, 0). With
    // the rule check merged into `update_block`, the synchronous
    // placement path (`set_block_with_state(_, _, _, true)`) should
    // fire the FF immediately — no separate `process_block_updates`
    // pump required.
    let (_scratch, mut world, base) = lcm3_world("placement-fires");
    // Pick coords at chunk-y = 0 so the test is independent of where
    // generated terrain happens to sit; clear with explicit air first
    // in case generation dropped solid terrain there.
    let wire_coord = Vec3i::new(0, 0, 0);
    let ff_coord = Vec3i::new(0, 1, 0);
    world.set_block(ff_coord, base.air, true);
    world.set_block(wire_coord, base.air, true);
    world.process_block_updates();
    world.set_block_with_state(wire_coord, base.lcm3_wire, lcm3_state(0, 1, 0), true);
    // Drain the wire's placement-induced enqueues so the FF placement
    // is the only event we're testing the trigger from.
    world.process_block_updates();
    assert_eq!(
        world.block(ff_coord).expect("loaded").id,
        base.air,
        "FF coord should still be air pre-placement"
    );

    world.set_block_with_state(ff_coord, base.lcm3_ff, lcm3_state(0, 0, 0), true);

    // Don't call `process_block_updates` — the FF should already have
    // fired during its own placement, since `set_block_with_state`
    // synchronously calls `update_block(_, true)` which now also runs
    // the LCM3 rule on the placed cell.
    let ff = world.block(ff_coord).expect("loaded");
    assert_eq!(lcm3_data(ff.state), 1, "FF data = wire.data after placement");
    assert_eq!(lcm3_clock(ff.state), 1, "FF clock advanced 0 → 1 on placement");
}

#[test]
fn lcm3_register_rule_advances_with_input() {
    // Wire (cap +Y, data 1, clock 0) at (0, 0, 0).
    // FF   (cap +Y, data 0, clock 0) at (0, 1, 0).
    // FF reads from -Y (the wire below); rule requires input at FF's
    // own clock (= 0). After one `process_block_updates` (which runs
    // both lighting and the LCM3 rule per drained cell), FF should hold
    // the wire's data and have advanced to clock 1.
    let (_scratch, mut world, base) = lcm3_world("ff-rule");
    let wire_coord = Vec3i::new(0, 0, 0);
    let ff_coord = Vec3i::new(0, 1, 0);
    // `queue_update = true` so each cell's neighbours land in the
    // block-update queue — that's what `process_block_updates` drains.
    world.set_block_with_state(wire_coord, base.lcm3_wire, lcm3_state(0, 1, 0), true);
    world.set_block_with_state(ff_coord, base.lcm3_ff, lcm3_state(0, 0, 0), true);

    world.process_block_updates();

    let ff = world.block(ff_coord).expect("loaded");
    assert_eq!(lcm3_orientation(ff.state), 0, "orientation preserved");
    assert_eq!(lcm3_data(ff.state), 1, "FF data = wire.data");
    assert_eq!(lcm3_clock(ff.state), 1, "FF clock advanced 0 → 1");

    // Wire never had its own input, so it shouldn't have fired.
    let wire = world.block(wire_coord).expect("loaded");
    assert_eq!(wire.state, lcm3_state(0, 1, 0), "wire stays put");
}

#[test]
fn lcm3_not_gate_inverts_input() {
    // Wire (cap +Y, data 0, clock 1) at (0, 0, 0).
    // NOT  (cap +Y, data 0, clock 0) at (0, 1, 0).
    // Gate rule requires input at NOT's clock+1 = 1; wire is at 1. ✓
    // After tick, NOT should hold `1 - wire.data = 1` at clock 1.
    let (_scratch, mut world, base) = lcm3_world("not-rule");
    let wire_coord = Vec3i::new(0, 0, 0);
    let not_coord = Vec3i::new(0, 1, 0);
    world.set_block_with_state(wire_coord, base.lcm3_wire, lcm3_state(0, 0, 1), true);
    world.set_block_with_state(not_coord, base.lcm3_not, lcm3_state(0, 0, 0), true);

    world.process_block_updates();

    let not = world.block(not_coord).expect("loaded");
    assert_eq!(lcm3_data(not.state), 1, "NOT data = !wire.data");
    assert_eq!(lcm3_clock(not.state), 1, "NOT clock advanced");
}

#[test]
fn lcm3_and_gate_takes_all_side_inputs() {
    // AND (cap +Y) at (0, 0, 0). Two side inputs:
    //   - Wire E (cap -X, data 1, clock 1) at (1, 0, 0) — outputs west.
    //   - Wire W (cap +X, data 1, clock 1) at (-1, 0, 0) — outputs east.
    // Both feed AND's east + west side faces (which are IN ports).
    // AND fires with data = 1 AND 1 = 1.
    let (_scratch, mut world, base) = lcm3_world("and-rule");
    let and_coord = Vec3i::new(0, 0, 0);
    let east_wire_coord = Vec3i::new(1, 0, 0);
    let west_wire_coord = Vec3i::new(-1, 0, 0);
    // Orientation index: 2 = +X, 3 = -X (see `Orientation::for_axis_aligned_index`).
    world.set_block_with_state(east_wire_coord, base.lcm3_wire, lcm3_state(3, 1, 1), true);
    world.set_block_with_state(west_wire_coord, base.lcm3_wire, lcm3_state(2, 1, 1), true);
    world.set_block_with_state(and_coord, base.lcm3_and, lcm3_state(0, 0, 0), true);

    world.process_block_updates();

    let and = world.block(and_coord).expect("loaded");
    assert_eq!(lcm3_data(and.state), 1, "AND(1, 1) = 1");
    assert_eq!(lcm3_clock(and.state), 1);

    // Second test: flip one input to data=0 → AND should produce 0.
    world.set_block_with_state(east_wire_coord, base.lcm3_wire, lcm3_state(3, 0, 1), true);
    world.set_block_with_state(and_coord, base.lcm3_and, lcm3_state(0, 0, 0), true);
    world.process_block_updates();
    let and = world.block(and_coord).expect("loaded");
    assert_eq!(lcm3_data(and.state), 0, "AND(0, 1) = 0");
}

#[test]
fn lcm3_clock_rate_caps_self_clocking_loop() {
    // Six-cell self-clocking loop in the X-Y plane at z=0 — see
    // `docs/block_updates.md` for the topology rationale. After each
    // tick, every cell advances by `min(loop_capacity, lcm3_clock_rate)`
    // clock steps. Loop capacity is unbounded for this geometry, so the
    // FF cap is the binding constraint: the FF (and via the chain,
    // every other cell) advances by exactly `lcm3_clock_rate` clocks
    // per tick.
    //
    // Cells (clockwise around the loop):
    //   FF (0, 0,  0) cap +Y          — register
    //   F1 (0, 1,  0) cap +Y          — fork (sends data east)
    //   F2 (1, 1,  0) cap +X          — fork (sends data south)
    //   W1 (1, 0,  0) cap -Y          — wire (carries data south)
    //   F3 (1,-1,  0) cap -Y          — fork (sends data west)
    //   F4 (0,-1,  0) cap -X          — fork (sends data north into FF.IN)
    let (_scratch, mut world, base) = lcm3_world("ff-cap");
    let ff = Vec3i::new(0, 0, 0);
    let f1 = Vec3i::new(0, 1, 0);
    let f2 = Vec3i::new(1, 1, 0);
    let w1 = Vec3i::new(1, 0, 0);
    let f3 = Vec3i::new(1, -1, 0);
    let f4 = Vec3i::new(0, -1, 0);
    world.set_block_with_state(ff, base.lcm3_ff, lcm3_state(0, 0, 0), true);
    world.set_block_with_state(f1, base.lcm3_fork, lcm3_state(0, 0, 0), true);
    world.set_block_with_state(f2, base.lcm3_fork, lcm3_state(2, 0, 0), true);
    world.set_block_with_state(w1, base.lcm3_wire, lcm3_state(1, 0, 0), true);
    world.set_block_with_state(f3, base.lcm3_fork, lcm3_state(1, 0, 0), true);
    world.set_block_with_state(f4, base.lcm3_fork, lcm3_state(3, 0, 0), true);

    // Clock rate 1: one full loop advance per tick.
    world.set_lcm3_clock_rate(1);
    world.process_block_updates();
    for &c in &[ff, f1, f2, w1, f3, f4] {
        assert_eq!(
            lcm3_clock(world.block(c).expect("loaded").state),
            1,
            "cell {c:?} should be at clock 1 after tick (rate=1)"
        );
    }

    // Tick again — system should advance to clock 2. The previous tick
    // re-enqueued every cell's neighbours, so the queue is non-empty
    // entering this call (LCM3 rules cascade through repeated drains).
    world.process_block_updates();
    for &c in &[ff, f1, f2, w1, f3, f4] {
        assert_eq!(
            lcm3_clock(world.block(c).expect("loaded").state),
            2,
            "cell {c:?} should be at clock 2 after second tick"
        );
    }

    // Clock rate 2: two loop advances per tick. From clock 2, two
    // advances → clock 4 mod 3 = clock 1.
    world.set_lcm3_clock_rate(2);
    world.process_block_updates();
    for &c in &[ff, f1, f2, w1, f3, f4] {
        assert_eq!(
            lcm3_clock(world.block(c).expect("loaded").state),
            1,
            "cell {c:?} should be at clock 1 after rate=2 tick"
        );
    }

    // Clock rate 0: pause — no cells should advance. (We poke the FF
    // to seed the queue; the rule simply doesn't fire because the cap
    // is `0`.)
    world.set_lcm3_clock_rate(0);
    let snapshot: Vec<_> = [ff, f1, f2, w1, f3, f4]
        .iter()
        .map(|&c| (c, world.block(c).expect("loaded").state))
        .collect();
    for &(c, _) in &snapshot {
        // Re-enqueue neighbours so the rule check actually runs against
        // each cell. Without this the queue would be empty and we'd be
        // testing "no work happens when queue is empty", not "no work
        // happens when cap is 0".
        world.update_block(c, true);
    }
    world.process_block_updates();
    for (c, expected) in snapshot {
        assert_eq!(
            world.block(c).expect("loaded").state,
            expected,
            "cell {c:?} should not change with clock rate 0"
        );
    }
}
