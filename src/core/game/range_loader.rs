//! `RangeLoader` — streaming policy that keeps a chunk-window around a
//! moving centre resident in the [`World`] chunk store.
//!
//! Owns:
//! - The chunk-coord centre + `render_distance` (in chunks).
//! - A [`LoadedCore`] optimisation that skips already-loaded shells in
//!   the bounded shell scan.
//! - A `HashMap<Vec3i, Lease>` keeping every chunk in the load
//!   window pinned. A `Lease` is the canonical pin in the new
//!   chunk-store protocol: while it's alive, an `unload_chunk` for
//!   that coord blocks in its drain step, so the world cannot
//!   evict a chunk we still need.
//!
//! Drives the `World`:
//! - `set_center` updates the centre.
//! - `tick_chunk_loading` issues loads for newly-in-window chunks and
//!   unloads for newly-out-of-window chunks (bounded heaps,
//!   `MAX_CHUNK_LOADS` / `MAX_CHUNK_UNLOADS` per call). Loads run
//!   synchronously: `World::install_chunk` tries disk first; on miss
//!   it invokes the caller-supplied closure, which calls back into
//!   [`TerrainGenerator::build_blocks`].

use std::cmp::{Ordering, Reverse};
use std::collections::{BinaryHeap, HashMap};

use crate::core::game::worldgen::TerrainGenerator;
use crate::core::math::Vec3i;
use crate::core::world::{Lease, World, chunk_coord};

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
//   RangeLoader
// ----------------------------------------------------------------------

pub struct RangeLoader {
    /// Chunk-coord centre.
    center: Vec3i,
    /// Render distance in chunks. The load window is
    /// `render_distance + LOAD_RADIUS_BUFFER`.
    render_distance: i32,
    loaded_distance: i32,
    pins: HashMap<Vec3i, Lease>,
    /// HUD counter — wraps on overflow; only ever read for display.
    unloaded_chunks: u32,
}

impl RangeLoader {
    pub fn new(render_distance: i32) -> Self {
        Self {
            center: Vec3i::new(0, 0, 0),
            render_distance,
            loaded_distance: -1,
            pins: HashMap::new(),
            unloaded_chunks: 0,
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

    /// Total chunks evicted by this loader since construction (wraps).
    pub fn unloaded_chunks(&self) -> u32 {
        self.unloaded_chunks
    }

    /// Update the load-window centre. Shrinks the `LoadedCore` by the
    /// chebyshev distance moved (blocks inside the new ball are still
    /// a subset of the old ball). Block-coord input — floored to the
    /// containing chunk.
    pub fn set_center(&mut self, center_block: Vec3i) {
        let center_chunk = chunk_coord(center_block);
        let mv = center_chunk - self.center;
        let mv_cheb = mv.x.abs().max(mv.y.abs()).max(mv.z.abs());
        self.loaded_distance = (self.loaded_distance - mv_cheb).max(-1);
        self.center = center_chunk;
    }

    /// Synchronously load chunks within the window and unload ones
    /// outside.
    pub fn tick_chunk_loading(&mut self, world: &World, terrain_gen: &mut TerrainGenerator) {
        for cc in self.collect_load_candidates(world) {
            if world.is_loaded(cc) {
                continue;
            }
            world.load_chunk(cc, || terrain_gen.build_blocks(cc));
            world.mark_neighbour_chunks_updated(cc);
            if let Some(lease) = world.try_acquire_lease(cc) {
                self.pins.insert(cc, lease);
            }
        }
        for cc in self.collect_unload_candidates(world) {
            self.unload_one(world, cc);
        }
    }

    // ---- internal ----

    fn collect_load_candidates(&mut self, world: &World) -> Vec<Vec3i> {
        let center = self.center;
        let load_dist = self.render_distance + LOAD_RADIUS_BUFFER;
        let start = self.loaded_distance + 1;
        let mut heap: BinaryHeap<ByDist> = BinaryHeap::with_capacity(MAX_CHUNK_LOADS + 1);
        let mut new_radius = self.loaded_distance;

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

        self.loaded_distance = new_radius;
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
        if world.is_loaded(cc) {
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
        for coord in world.loaded_coords() {
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

    fn unload_one(&mut self, world: &World, cc: Vec3i) {
        // Drop our lease first so World::unload_chunk's drain step
        // doesn't deadlock against our own pin. The eviction
        // state-machine then runs to completion (CAS → wait_drain
        // → flush → remove).
        self.pins.remove(&cc);
        world.unload_chunk(cc);
        self.unloaded_chunks = self.unloaded_chunks.wrapping_add(1);
        self.shrink_loaded_distance_for_unload(cc);
    }

    fn shrink_loaded_distance_for_unload(&mut self, cc: Vec3i) {
        if self.loaded_distance < 0 {
            return;
        }
        let d = cc - self.center;
        let dist = d.x.abs().max(d.y.abs()).max(d.z.abs());
        self.loaded_distance = self.loaded_distance.min(dist - 1);
    }
}
