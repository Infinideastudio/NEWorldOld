//! Tests for the parent `world` module.
//!
//! Tests for sub-modules live next to the code they exercise — `grid::tests`
//! for [`super::ChunkGrid`], `store::tests` for [`super::TilesStore`].
//! Shared `TEST_LOCK` + `ScratchDir` come from [`super::test_support`].

#![cfg(test)]

use std::sync::Arc;

use super::*;
use super::test_support::{ScratchDir, TEST_LOCK};
use crate::blocks::{BlockRegistry, register_base_blocks};
use crate::math::Vec3d;

fn make_registry() -> (Arc<BlockRegistry>, BaseBlocks) {
    let mut r = BlockRegistry::new();
    let base = register_base_blocks(&mut r);
    (Arc::new(r), base)
}

fn build_world(name: &str, render_distance: i32) -> World {
    let (registry, base) = make_registry();
    World::new(name.to_owned(), render_distance, 0, registry, base).expect("world::new")
}

// ---------- coord helpers ----------

#[test]
fn chunk_coord_negative_arithmetic_shift() {
    // SIZE_LOG = 4, SIZE = 16. -1 >> 4 == -1.
    assert_eq!(chunk_coord(Vec3i::new(0, 0, 0)), Vec3i::new(0, 0, 0));
    assert_eq!(chunk_coord(Vec3i::new(15, 15, 15)), Vec3i::new(0, 0, 0));
    assert_eq!(chunk_coord(Vec3i::new(16, 16, 16)), Vec3i::new(1, 1, 1));
    assert_eq!(chunk_coord(Vec3i::new(-1, -1, -1)), Vec3i::new(-1, -1, -1));
    assert_eq!(chunk_coord(Vec3i::new(-16, -16, -16)), Vec3i::new(-1, -1, -1));
    assert_eq!(chunk_coord(Vec3i::new(-17, -17, -17)), Vec3i::new(-2, -2, -2));
}

#[test]
fn block_coord_modulo_bitmask() {
    assert_eq!(block_coord(Vec3i::new(0, 0, 0)), Vec3::<u32>::new(0, 0, 0));
    assert_eq!(block_coord(Vec3i::new(15, 7, 3)), Vec3::<u32>::new(15, 7, 3));
    assert_eq!(block_coord(Vec3i::new(16, 16, 16)), Vec3::<u32>::new(0, 0, 0));
    // -1 in two's-complement is ...11111111 → low 4 bits = 15.
    assert_eq!(
        block_coord(Vec3i::new(-1, -1, -1)),
        Vec3::<u32>::new(15, 15, 15)
    );
    assert_eq!(
        block_coord(Vec3i::new(-16, -16, -16)),
        Vec3::<u32>::new(0, 0, 0)
    );
}

// ---------- World ----------

#[test]
fn world_set_block_then_block_round_trips() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("set-block");
    let mut w = build_world("set-block", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(1, 2, 3);
    let stone = w.base_blocks.stone;
    w.set_block(coord, stone, false);
    let got = w.block(coord).expect("loaded");
    assert_eq!(got.id, stone);
    let cc = chunk_coord(coord);
    let chunk = w.chunk(cc).expect("loaded chunk");
    assert!(chunk.modified());
}

#[test]
fn world_block_or_air_returns_air_for_unloaded_coord() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("air");
    let w = build_world("air", 1);
    let far_off = Vec3i::new(100_000, 100_000, 100_000);
    let b = w.block_or_air(far_off);
    assert_eq!(b.id, w.base_blocks.air);
    assert!(w.block(far_off).is_none());
}

#[test]
fn world_chunk_and_chunk_by_coord_agree_inside_window() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("agree");
    let mut w = build_world("agree", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let cc = Vec3i::new(0, 0, 0);
    let by_grid = w.chunk(cc).expect("via grid");
    let by_map = w.chunk_by_coord(cc).expect("via map");
    assert_eq!(by_grid.coord(), by_map.coord());
    assert_eq!(by_grid.coord(), cc);
}

#[test]
fn world_chunk_grid_drops_after_slide_but_by_coord_stays() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("slide");
    let mut w = build_world("slide", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let cc = Vec3i::new(0, 0, 0);
    assert!(w.chunk(cc).is_some());
    assert!(w.chunk_by_coord(cc).is_some());

    // Slide the grid so `cc` falls outside the new window without an unload.
    let far = Vec3i::new(10_000, 0, 10_000);
    w.set_center(far * Chunk::SIZE);
    assert!(w.chunk(cc).is_none());
    assert!(w.chunk_by_coord(cc).is_some());
}

#[test]
fn world_update_block_skips_when_neighbours_unloaded() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("update-skip");
    let mut w = build_world("update-skip", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    // Load only the centre chunk; its +x neighbour stays unloaded.
    w.load_chunk(Vec3i::new(0, 0, 0));
    let coord = Vec3i::new(15, 5, 5);
    assert!(!w.update_block(coord, true));
    assert!(w.block_update_queue().is_empty());
}

#[test]
fn world_update_block_queues_neighbour_updates_when_all_loaded() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("update-queue");
    let mut w = build_world("update-queue", 1);
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
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("idempotent");
    let mut w = build_world("idempotent", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let n1 = w.chunks.len();
    w.tick_chunk_loading();
    let n2 = w.chunks.len();
    assert_eq!(n1, n2, "second tick should not double-load");
    assert_eq!(w.by_coord.len(), w.chunks.len());
}

#[test]
fn async_pipeline_round_trip_matches_sync_load() {
    // Reference: synchronously load, capture the target chunk's bytes.
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("async-roundtrip");
    let target = Vec3i::new(0, 0, 0);

    let reference_bytes = {
        let mut w = build_world("async-roundtrip-ref", 1);
        w.set_center(Vec3i::new(0, 0, 0));
        w.tick_chunk_loading();
        let chunk = w
            .chunk_by_coord(target)
            .expect("sync load should produce chunk");
        chunk.package_to()
    };
    assert!(
        !reference_bytes.is_empty(),
        "sync-loaded origin chunk should have data"
    );

    // Async pipeline: issue a load request and wait for the worker.
    let mut w = build_world("async-roundtrip", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading_async();
    let deadline = std::time::Instant::now() + std::time::Duration::from_secs(10);
    let mut inserted: Vec<Vec3i> = Vec::new();
    while std::time::Instant::now() < deadline {
        let mut got = w.poll_load_results();
        inserted.append(&mut got);
        if w.chunk_by_coord(target).is_some() {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(5));
    }
    assert!(
        inserted.contains(&target),
        "async load should have emitted target coord"
    );
    let chunk = w
        .chunk_by_coord(target)
        .expect("async load should install chunk");
    assert_eq!(chunk.package_to(), reference_bytes);
}

#[test]
fn block_view_for_world_forwards_to_inherent_methods() {
    let _guard = TEST_LOCK.lock().unwrap();
    let _scratch = ScratchDir::new("blockview");
    let mut w = build_world("blockview", 1);
    w.set_center(Vec3i::new(0, 0, 0));
    w.tick_chunk_loading();
    let coord = Vec3i::new(2, 3, 4);
    let inherent = World::block(&w, coord);
    let via_trait = <World as BlockView>::block(&w, coord);
    assert_eq!(inherent, via_trait);
    let inherent = World::block_or_air(&w, coord);
    let via_trait = <World as BlockView>::block_or_air(&w, coord);
    assert_eq!(inherent, via_trait);
    let aabb = Aabb3d::new(Vec3d::new(0.0, 0.0, 0.0), Vec3d::new(1.0, 1.0, 1.0));
    let inherent = World::hitboxes(&w, aabb);
    let via_trait = <World as BlockView>::hitboxes(&w, aabb);
    assert_eq!(inherent.len(), via_trait.len());
    let inherent = World::in_water(&w, aabb);
    let via_trait = <World as BlockView>::in_water(&w, aabb);
    assert_eq!(inherent, via_trait);
}
