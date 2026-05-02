//! `RangeLoader` — streaming policy that keeps a chunk-window around a
//! moving centre resident in the [`World`] chunk store.
//!
//! Owns:
//! - The chunk-coord centre + `render_distance` (in chunks).
//! - A [`LoadedCore`] optimisation that skips already-loaded shells in
//!   the bounded shell scan.
//! - A `HashSet<Vec3i>` of coords currently in flight on the async
//!   pipeline (so a coord doesn't get re-issued before its result lands).
//! - A `HashMap<Vec3i, Arc<Chunk>>` keeping every chunk in the load
//!   window pinned: the `Arc` itself is the pin — eviction drops the
//!   world's clone, our clone keeps the chunk alive until we drop it.
//!
//! Drives the `World`:
//! - `set_center` updates the centre.
//! - `tick_chunk_loading[_async]` issues loads for newly-in-window
//!   chunks and unloads for newly-out-of-window chunks (bounded heaps,
//!   `MAX_CHUNK_LOADS` / `MAX_CHUNK_UNLOADS` per call).
//! - `poll_load_results` drains finished pipeline loads, filters by
//!   window, calls `World::install_chunk` + pins.

use std::cmp::{Ordering, Reverse};
use std::collections::{BinaryHeap, HashMap, HashSet};
use std::sync::Arc;

use crate::core::game::pipeline::LoadResult;
use crate::core::game::worldgen::TerrainGenerator;
use crate::core::world::{Chunk, World, chunk_coord};
use crate::math::Vec3i;

// ----------------------------------------------------------------------
//   Tuning constants
// ----------------------------------------------------------------------

/// Maximum chunks dispatched on a single load tick.
pub const MAX_CHUNK_LOADS: usize = 64;
/// Maximum chunks evicted on a single load tick.
pub const MAX_CHUNK_UNLOADS: usize = 64;
/// Extra chunks kept resident outside the render distance — the
/// renderer needs the 3×3×3 neighbourhood loaded before it can mesh
/// a chunk, so the streaming window has to reach one further.
pub const LOAD_RADIUS_BUFFER: i32 = 1;

// ----------------------------------------------------------------------
//   ByDist — bounded-heap entry
// ----------------------------------------------------------------------

#[derive(Clone, Copy, PartialEq, Eq)]
struct ByDist {
    dist: i32,
    coord: Vec3i,
}

impl Ord for ByDist {
    fn cmp(&self, other: &Self) -> Ordering {
        self.dist.cmp(&other.dist)
    }
}

impl PartialOrd for ByDist {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

// ----------------------------------------------------------------------
//   LoadedCore
// ----------------------------------------------------------------------

/// Concentric region known to be fully loaded (or in-flight). Lets the
/// shell scan skip the inner ball; only the outermost shell needs
/// scanning per call once the window stabilises.
///
/// Invariant: every chunk coord with chebyshev distance ≤ `radius` from
/// `RangeLoader::center` is either present in `World` or has a pending
/// in-flight load. `radius == -1` ⇒ "nothing known yet".
#[derive(Clone, Copy)]
struct LoadedCore {
    radius: i32,
}

impl Default for LoadedCore {
    fn default() -> Self {
        Self { radius: -1 }
    }
}

// ----------------------------------------------------------------------
//   RangeLoader
// ----------------------------------------------------------------------

pub struct RangeLoader {
    /// Chunk-coord centre.
    center: Vec3i,
    /// Render distance in chunks. The load window is
    /// `render_distance + LOAD_RADIUS_BUFFER`.
    render_distance: i32,
    loaded_core: LoadedCore,
    in_flight: HashSet<Vec3i>,
    pins: HashMap<Vec3i, Arc<Chunk>>,
}

impl RangeLoader {
    pub fn new(render_distance: i32) -> Self {
        Self {
            center: Vec3i::new(0, 0, 0),
            render_distance,
            loaded_core: LoadedCore::default(),
            in_flight: HashSet::new(),
            pins: HashMap::new(),
        }
    }

    pub fn center_ccoord(&self) -> Vec3i {
        self.center
    }

    pub fn render_distance(&self) -> i32 {
        self.render_distance
    }

    pub fn set_render_distance(&mut self, distance: i32) {
        self.render_distance = distance;
    }

    /// Update the load-window centre. Shrinks the `LoadedCore` by the
    /// chebyshev distance moved (blocks inside the new ball are still
    /// a subset of the old ball). Block-coord input — floored to the
    /// containing chunk.
    pub fn set_center(&mut self, center_block: Vec3i) {
        let center_chunk = chunk_coord(center_block);
        let mv = center_chunk - self.center;
        let mv_cheb = mv.x.abs().max(mv.y.abs()).max(mv.z.abs());
        self.loaded_core.radius = (self.loaded_core.radius - mv_cheb).max(-1);
        self.center = center_chunk;
    }

    /// Synchronously load chunks within the window and unload ones
    /// outside.
    pub fn tick_chunk_loading(&mut self, world: &mut World, terrain_gen: &mut TerrainGenerator) {
        for cc in self.collect_load_candidates(world) {
            if world.is_loaded(cc) {
                continue;
            }
            let (blocks, from_disk) = terrain_gen.build_chunk_sync(world, cc);
            world.install_chunk(cc, blocks, from_disk);
            world.mark_neighbour_chunks_updated(cc);
            if let Some(arc) = world.chunk(cc) {
                self.pins.insert(cc, arc);
            }
        }
        for cc in self.collect_unload_candidates(world) {
            self.unload_one(world, terrain_gen, cc, /*async_save=*/ false);
            world.unloaded_chunks = world.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Async sibling: dispatches load requests to the pipeline; never
    /// blocks. Pair with [`Self::poll_load_results`].
    pub fn tick_chunk_loading_async(
        &mut self,
        world: &mut World,
        terrain_gen: &mut TerrainGenerator,
    ) {
        for cc in self.collect_load_candidates(world) {
            if terrain_gen.request_load(cc) {
                self.in_flight.insert(cc);
            }
        }
        for cc in self.collect_unload_candidates(world) {
            self.unload_one(world, terrain_gen, cc, /*async_save=*/ true);
            world.unloaded_chunks = world.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Drain finished pipeline loads, install in-window ones into the
    /// world (and pin them), return the list of inserted coords.
    pub fn poll_load_results(
        &mut self,
        world: &mut World,
        terrain_gen: &mut TerrainGenerator,
    ) -> Vec<Vec3i> {
        let drained = terrain_gen.drain_results();
        let mut inserted = Vec::with_capacity(drained.len());
        for LoadResult {
            coord,
            blocks,
            from_disk,
        } in drained
        {
            self.in_flight.remove(&coord);
            let half = self.render_distance + LOAD_RADIUS_BUFFER;
            let d = coord - self.center;
            let in_window = d.x.abs() <= half && d.y.abs() <= half && d.z.abs() <= half;
            if !in_window || world.is_loaded(coord) {
                continue;
            }
            world.install_chunk(coord, blocks, from_disk);
            world.mark_neighbour_chunks_updated(coord);
            if let Some(arc) = world.chunk(coord) {
                self.pins.insert(coord, arc);
            }
            inserted.push(coord);
        }
        inserted
    }

    // ---- internal ----

    fn collect_load_candidates(&mut self, world: &World) -> Vec<Vec3i> {
        let center = self.center;
        let load_dist = self.render_distance + LOAD_RADIUS_BUFFER;
        let start = self.loaded_core.radius + 1;
        let mut heap: BinaryHeap<ByDist> = BinaryHeap::with_capacity(MAX_CHUNK_LOADS + 1);
        let mut new_radius = self.loaded_core.radius;

        'shells: for r in start..=load_dist {
            for dx in -r..=r {
                for dy in -r..=r {
                    let xy_boundary = dx.abs() == r || dy.abs() == r;
                    if xy_boundary {
                        for dz in -r..=r {
                            self.try_push_load_candidate(
                                &mut heap,
                                center + Vec3i::new(dx, dy, dz),
                                center,
                                world,
                            );
                        }
                    } else {
                        for &dz in &[-r, r] {
                            self.try_push_load_candidate(
                                &mut heap,
                                center + Vec3i::new(dx, dy, dz),
                                center,
                                world,
                            );
                        }
                    }
                }
            }
            if heap.is_empty() {
                new_radius = r;
            }
            if heap.len() == MAX_CHUNK_LOADS {
                break 'shells;
            }
        }

        self.loaded_core.radius = new_radius;
        heap.into_sorted_vec()
            .into_iter()
            .map(|e| e.coord)
            .collect()
    }

    fn try_push_load_candidate(
        &self,
        heap: &mut BinaryHeap<ByDist>,
        cc: Vec3i,
        center: Vec3i,
        world: &World,
    ) {
        if world.is_loaded(cc) || self.in_flight.contains(&cc) {
            return;
        }
        let rel = cc - center;
        let dist = rel.x * rel.x + rel.y * rel.y + rel.z * rel.z;
        heap.push(ByDist { dist, coord: cc });
        if heap.len() > MAX_CHUNK_LOADS {
            heap.pop();
        }
    }

    fn collect_unload_candidates(&self, world: &World) -> Vec<Vec3i> {
        let center = self.center;
        let unload_dist = self.render_distance + LOAD_RADIUS_BUFFER;
        let mut heap: BinaryHeap<Reverse<ByDist>> =
            BinaryHeap::with_capacity(MAX_CHUNK_UNLOADS + 1);
        for coord in world.loaded_iter() {
            let d = coord - center;
            if d.x.abs() > unload_dist || d.y.abs() > unload_dist || d.z.abs() > unload_dist {
                let dist = d.x * d.x + d.y * d.y + d.z * d.z;
                heap.push(Reverse(ByDist { dist, coord }));
                if heap.len() > MAX_CHUNK_UNLOADS {
                    heap.pop();
                }
            }
        }
        heap.into_sorted_vec()
            .into_iter()
            .map(|Reverse(e)| e.coord)
            .collect()
    }

    fn unload_one(
        &mut self,
        world: &mut World,
        terrain_gen: &TerrainGenerator,
        cc: Vec3i,
        async_save: bool,
    ) {
        // Drop our pin clone first so the chunk's only remaining
        // strong reference (after world.evict) is the world's, which
        // we're about to remove.
        self.pins.remove(&cc);
        if let Some((bytes, captured_gen)) = world.package_if_dirty(cc) {
            if async_save {
                terrain_gen.request_save(cc, bytes);
            } else {
                let _ = world.save_chunk_bytes(cc, &bytes);
            }
            world.advance_save_gen(cc, captured_gen);
        }
        world.evict(cc);
        self.shrink_loaded_core_for_unload(cc);
    }

    fn shrink_loaded_core_for_unload(&mut self, cc: Vec3i) {
        if self.loaded_core.radius < 0 {
            return;
        }
        let d = cc - self.center;
        let dist = d.x.abs().max(d.y.abs()).max(d.z.abs());
        self.loaded_core.radius = self.loaded_core.radius.min(dist - 1);
    }
}
