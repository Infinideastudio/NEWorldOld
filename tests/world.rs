//! Integration tests for `crate::worlds::World`.
//!
//! Moved out of `src/worlds/world/tests.rs` so test cases run against the
//! published API only and don't need to chdir into a scratch directory.
//! Each test gets its own absolute scratch path via [`common::ScratchDir`]
//! and constructs the world with [`World::new_at`].

mod common;

use std::sync::Arc;

use neworld::blocks::{BaseBlocks, BlockRegistry, register_base_blocks};
use neworld::chunks::Chunk;
use neworld::math::{Aabbd, Vec3d, Vec3i, Vec3u};
use neworld::worlds::{BlockView, World, block_coord, chunk_coord};

use common::ScratchDir;

fn make_registry() -> (Arc<BlockRegistry>, BaseBlocks) {
    let mut r = BlockRegistry::new();
    let base = register_base_blocks(&mut r);
    (Arc::new(r), base)
}

/// Build a world rooted at `scratch` with a deterministic seed.
fn build_world(scratch: &ScratchDir, name: &str, render_distance: i32) -> (World, BaseBlocks) {
    let (registry, base) = make_registry();
    let world = World::new_at(
        scratch.path(),
        name.to_owned(),
        render_distance,
        0,
        registry,
        base,
    )
    .expect("World::new_at");
    (world, base)
}

// ---------- coord helpers ----------

#[test]
fn chunk_coord_negative_arithmetic_shift() {
    // SIZE_LOG = 4, SIZE = 16. -1 >> 4 == -1.
    assert_eq!(chunk_coord(Vec3i::new(0, 0, 0)), Vec3i::new(0, 0, 0));
    assert_eq!(chunk_coord(Vec3i::new(15, 15, 15)), Vec3i::new(0, 0, 0));
    assert_eq!(chunk_coord(Vec3i::new(16, 16, 16)), Vec3i::new(1, 1, 1));
    assert_eq!(chunk_coord(Vec3i::new(-1, -1, -1)), Vec3i::new(-1, -1, -1));
    assert_eq!(
        chunk_coord(Vec3i::new(-16, -16, -16)),
        Vec3i::new(-1, -1, -1)
    );
    assert_eq!(
        chunk_coord(Vec3i::new(-17, -17, -17)),
        Vec3i::new(-2, -2, -2)
    );
}

#[test]
fn block_coord_modulo_bitmask() {
    assert_eq!(block_coord(Vec3i::new(0, 0, 0)), Vec3u::new(0, 0, 0));
    assert_eq!(block_coord(Vec3i::new(15, 7, 3)), Vec3u::new(15, 7, 3));
    assert_eq!(block_coord(Vec3i::new(16, 16, 16)), Vec3u::new(0, 0, 0));
    // -1 in two's-complement is ...11111111 → low 4 bits = 15.
    assert_eq!(block_coord(Vec3i::new(-1, -1, -1)), Vec3u::new(15, 15, 15));
    assert_eq!(block_coord(Vec3i::new(-16, -16, -16)), Vec3u::new(0, 0, 0));
}

// ---------- World ----------

#[test]
fn world_set_block_then_block_round_trips() {
    let scratch = ScratchDir::new("set-block");
    let (mut w, base) = build_world(&scratch, "set-block", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(1, 2, 3);
    w.set_block(coord, base.stone, false);
    let got = w.block(coord).expect("loaded");
    assert_eq!(got.id, base.stone);
    let cc = chunk_coord(coord);
    let chunk = w.chunk(cc).expect("loaded chunk");
    assert!(chunk.modified());
}

#[test]
fn world_block_or_air_returns_air_for_unloaded_coord() {
    let scratch = ScratchDir::new("air");
    let (w, base) = build_world(&scratch, "air", 1);
    let far_off = Vec3i::new(100_000, 100_000, 100_000);
    let b = w.block_or_air(far_off);
    assert_eq!(b.id, base.air);
    assert!(w.block(far_off).is_none());
}

#[test]
fn world_chunk_returns_loaded_chunk_at_coord() {
    let scratch = ScratchDir::new("chunk-lookup");
    let (mut w, _base) = build_world(&scratch, "chunk-lookup", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let cc = Vec3i::new(0, 0, 0);
    let chunk = w.chunk(cc).expect("loaded chunk");
    assert_eq!(chunk.coord(), cc);
    assert!(w.is_loaded(cc));
}

#[test]
fn world_set_center_does_not_unload_existing_chunks() {
    // `set_center` only updates the centre + height-map cache; load /
    // unload is `tick_chunk_loading`'s job. After a slide far past the old
    // chunk, the chunk must still be reachable via `chunk` until the next
    // `tick_chunk_loading` reaps it.
    let scratch = ScratchDir::new("set-center");
    let (mut w, _base) = build_world(&scratch, "set-center", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let cc = Vec3i::new(0, 0, 0);
    assert!(w.is_loaded(cc));

    // Slide far away — origin chunk is now outside the window, but no
    // unload tick has happened yet.
    let far = Vec3i::new(10_000, 0, 10_000);
    w.set_center(far * Chunk::SIZE);
    assert!(w.is_loaded(cc), "set_center alone must not unload");

    // After ticking, the now-distant chunk gets reaped.
    w.tick_chunk_loading();
    assert!(!w.is_loaded(cc), "tick_chunk_loading should unload it");
}

#[test]
fn world_update_block_skips_when_neighbours_unloaded() {
    let scratch = ScratchDir::new("update-skip");
    // `render_distance = 0` + the load buffer (1) loads chunks at chunk
    // coords `-1..=1` on every axis. Block coord (31, 5, 5) sits in chunk
    // (1, 0, 0) — its +x neighbour at (32, 5, 5) is in chunk (2, 0, 0)
    // which stays unloaded, so `update_block` must bail.
    let (mut w, _base) = build_world(&scratch, "update-skip", 0);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(31, 5, 5);
    assert!(!w.update_block(coord, true));
    assert!(w.block_update_queue().is_empty());
}

#[test]
fn world_update_block_queues_neighbour_updates_when_all_loaded() {
    let scratch = ScratchDir::new("update-queue");
    let (mut w, _base) = build_world(&scratch, "update-queue", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(2, 3, 4);
    let drained_before = w.block_update_queue().len();
    let ok = w.update_block(coord, true);
    assert!(ok, "neighbours should be loaded");
    assert_eq!(
        w.block_update_queue().len() - drained_before,
        6,
        "expected 6 neighbour updates queued"
    );
}

#[test]
fn world_tick_chunk_loading_is_idempotent() {
    // `render_distance + LOAD_RADIUS_BUFFER` = 2 chunks → 5³=125 candidates,
    // beyond `MAX_CHUNK_LOADS = 64` per tick. Drain to a steady state, then
    // assert one more tick adds nothing.
    let scratch = ScratchDir::new("idempotent");
    let (mut w, _base) = build_world(&scratch, "idempotent", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    let mut prev = 0;
    for _ in 0..16 {
        w.tick_chunk_loading();
        if w.loaded_count() == prev {
            break;
        }
        prev = w.loaded_count();
    }
    let n1 = w.loaded_count();
    w.tick_chunk_loading();
    let n2 = w.loaded_count();
    assert_eq!(n1, n2, "post-stable tick should not double-load");
}

#[test]
fn async_pipeline_round_trip_matches_sync_load() {
    // Reference: synchronously load, capture the target chunk's bytes.
    let scratch = ScratchDir::new("async-roundtrip");
    let target = Vec3i::new(0, 0, 0);

    let reference_bytes = {
        let (mut w, _base) = build_world(&scratch, "async-roundtrip-ref", 1);
        w.set_center(Vec3i::new(0, 0, 0));
        w.tick_chunk_loading();
        let chunk = w.chunk(target).expect("sync load should produce chunk");
        chunk.package_to(&[])
    };
    assert!(
        !reference_bytes.is_empty(),
        "sync-loaded origin chunk should have data"
    );

    // Async pipeline: issue a load request and wait for the worker.
    let (mut w, _base) = build_world(&scratch, "async-roundtrip", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading_async();
    let deadline = std::time::Instant::now() + std::time::Duration::from_secs(10);
    let mut inserted: Vec<Vec3i> = Vec::new();
    while std::time::Instant::now() < deadline {
        let mut got = w.poll_load_results();
        inserted.append(&mut got);
        if w.chunk(target).is_some() {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(5));
    }
    assert!(
        inserted.contains(&target),
        "async load should have emitted target coord"
    );
    let chunk = w.chunk(target).expect("async load should install chunk");
    assert_eq!(chunk.package_to(&[]), reference_bytes);
}

#[test]
fn block_view_for_world_forwards_to_inherent_methods() {
    let scratch = ScratchDir::new("blockview");
    let (mut w, _base) = build_world(&scratch, "blockview", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(2, 3, 4);
    let inherent = World::block(&w, coord);
    let via_trait = <World as BlockView>::block(&w, coord);
    assert_eq!(inherent, via_trait);
    let inherent = World::block_or_air(&w, coord);
    let via_trait = <World as BlockView>::block_or_air(&w, coord);
    assert_eq!(inherent, via_trait);
    let aabb = Aabbd::new(Vec3d::new(0.0, 0.0, 0.0), Vec3d::new(1.0, 1.0, 1.0));
    let inherent = World::hitboxes(&w, aabb);
    let via_trait = <World as BlockView>::hitboxes(&w, aabb);
    assert_eq!(inherent.len(), via_trait.len());
    let inherent = World::in_water(&w, aabb);
    let via_trait = <World as BlockView>::in_water(&w, aabb);
    assert_eq!(inherent, via_trait);
}

#[test]
fn tiles_store_round_trips_raw_bytes() {
    use neworld::worlds::TilesStore;
    let scratch = ScratchDir::new("tiles");
    let store =
        TilesStore::open_at(&scratch.path().join("chunks.db")).expect("TilesStore::open_at");
    let coord = Vec3i::new(-2, 3, 17);
    let payload = b"hello-chunk-bytes-1234567890".to_vec();
    store.save(coord, &payload).expect("save");
    store.flush().expect("flush");
    let loaded = store.load(coord).expect("load");
    assert_eq!(loaded.as_deref(), Some(payload.as_slice()));
    let missing = store.load(Vec3i::new(99, 99, 99)).expect("load missing");
    assert!(missing.is_none());
}
