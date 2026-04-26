//! World — the canonical owner of every loaded chunk plus the supporting
//! `TilesStore`, sliding-window `ChunkGrid`, block-update queue, and player.
//!
//! Implements the design from `docs/rust_migration.md` §2.2 / §2.4 / §2.5 /
//! §4.6: chunks live in a `slab::Slab<Chunk>` arena, every loaded chunk is
//! also tracked in `by_coord: HashMap<Vec3i, ChunkKey>`, and the hot-path
//! `ChunkGrid` is a 3D sliding window of `Option<ChunkKey>`.
//!
//! Meshing (`list_render_chunks`, `render_chunks`, `RenderData`,
//! `ChunkRender`) is intentionally **out of scope for B4** — that lands in
//! [D]. When [D] arrives the slab will switch from `Slab<Chunk>` to
//! `Slab<ChunkSlot { chunk, render }>`; every method below that touches the
//! slab will route through the `chunk` / `chunk_mut` helpers, which are the
//! only call sites that need to be updated. Look for the `ChunkSlot`
//! comments below to find them.

mod error;
mod grid;
mod pipeline;
mod store;

pub use self::error::WorldError;
pub use self::grid::{ChunkGrid, ChunkKey};
pub use self::pipeline::{ChunkPipeline, LoadRequest, LoadResult};
pub use self::store::TilesStore;

use std::collections::{HashMap, HashSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::Arc;

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, Id, Light};
use crate::chunks::Chunk;
use crate::math::{Aabb3d, Vec3, Vec3d, Vec3i};
use crate::worlds::player::Player;
use crate::worldgen::{Generator, HeightMap};

/// Maximum number of chunk loads driven by one `tick_chunk_loading` call.
/// Mirrors C++ `worlds.ixx::MAX_CHUNK_LOADS`.
pub const MAX_CHUNK_LOADS: usize = 64;

/// Maximum number of chunk unloads driven by one `tick_chunk_loading` call.
/// Mirrors C++ `worlds.ixx::MAX_CHUNK_UNLOADS`.
pub const MAX_CHUNK_UNLOADS: usize = 64;

/// Maximum number of block updates drained per `process_block_updates` call.
/// Mirrors C++ `worlds.ixx::MAX_BLOCK_UPDATES`.
pub const MAX_BLOCK_UPDATES: usize = 65536;

// ----------------------------------------------------------------------
//   Coord helpers — port of `worlds.ixx::chunk_coord` / `block_coord`
// ----------------------------------------------------------------------

/// World coord → chunk coord. Uses Rust's arithmetic right shift on `i32`,
/// which matches C++20's signed-integer `>>` semantics.
#[must_use]
pub fn chunk_coord(coord: Vec3i) -> Vec3i {
    Vec3i::new(
        coord.x >> Chunk::SIZE_LOG,
        coord.y >> Chunk::SIZE_LOG,
        coord.z >> Chunk::SIZE_LOG,
    )
}

/// World coord → chunk-local coord (`coord & (SIZE - 1)` per axis), as `u32`.
/// Matches the C++ "signed-to-unsigned conversion implements modulo" trick
/// in `worlds.ixx::block_coord`.
#[must_use]
pub fn block_coord(coord: Vec3i) -> Vec3<u32> {
    let mask = (Chunk::SIZE - 1) as u32;
    Vec3::<u32>::new(
        (coord.x as u32) & mask,
        (coord.y as u32) & mask,
        (coord.z as u32) & mask,
    )
}

// `ChunkKey`, `ChunkGrid`, `TilesStore`, and `WorldError` live in submodules.

// ----------------------------------------------------------------------
//   BlockView — read-only block lookup trait used by player physics, etc.
// ----------------------------------------------------------------------

/// Read-only block lookup, used by player physics and other simulation
/// consumers. `World` provides the canonical implementation; future
/// "snapshot views" (e.g. mesh worker neighborhoods) will impl it too.
pub trait BlockView {
    fn block(&self, coord: Vec3i) -> Option<BlockData>;
    fn block_or_air(&self, coord: Vec3i) -> BlockData;
    fn hitboxes(&self, box_: Aabb3d) -> Vec<Aabb3d>;
    fn in_water(&self, box_: Aabb3d) -> bool;
}

// ----------------------------------------------------------------------
//   World
// ----------------------------------------------------------------------

pub struct World {
    name: String,
    /// Directory the world's files live in (`<root>/worlds/<name>/`). Used
    /// by [`Self::save_to_disk`] to write `player.bin` next to the chunk DB
    /// without depending on the process's cwd.
    dir: PathBuf,
    tiles_store: TilesStore,
    /// Canonical owner of every loaded chunk. **Future:** when [D] lands this
    /// becomes `slab::Slab<ChunkSlot { chunk: Chunk, render: ChunkRender }>`
    /// — `chunk(_mut)` and the `insert_chunk` / `remove_chunk` helpers below
    /// are the only call sites that need to be updated.
    chunks: slab::Slab<Chunk>,
    /// Every loaded chunk, regardless of whether it currently sits inside
    /// `chunk_grid`. Required because the loaded set is not always a subset
    /// of the grid window (future anchored chunks).
    by_coord: HashMap<Vec3i, ChunkKey>,
    chunk_grid: ChunkGrid,
    height_map: HeightMap,
    generator: Generator,
    /// Owned by the `World`; resolved at construction from the registry.
    /// `BaseBlocks` is `Copy` so we don't take an `Arc` for it (per the
    /// migration plan: it's a 19×u16 struct).
    base_blocks: BaseBlocks,
    /// Block registry. Cloned-`Arc` so the world can be torn down without
    /// holding the registry alive past the caller's lifetime.
    #[allow(dead_code)] // used once meshing arrives in [D]
    registry: Arc<BlockRegistry>,
    block_update_queue: VecDeque<Vec3i>,
    player: Player,
    game_time: u32,
    render_distance: i32,
    /// Center of the loaded region (in chunk coords). Tracked so that
    /// `tick_chunk_loading` can decide what to load/unload.
    center_ccoord: Vec3i,
    /// Async load/save pipeline (F5). One worker thread, `crossbeam-channel`
    /// transport. See `pipeline.rs`.
    pipeline: ChunkPipeline,
    /// Coords currently in flight on the pipeline. Prevents double-issuing
    /// loads for the same coord while the worker is busy with it.
    in_flight: HashSet<Vec3i>,
    pub unloaded_chunks: u32,
    pub updated_blocks: u32,
}

impl World {
    /// Open or create world `name` under the given `root` directory. The
    /// world's files live at `<root>/worlds/<name>/{chunks.db,player.bin}`.
    /// This is the path-explicit constructor — tests + production callers
    /// that already know where to put the world should use this instead of
    /// [`Self::new`] so they don't depend on cwd.
    ///
    /// `seed` seeds the terrain generator. `base_blocks` resolves the
    /// air/water/dirt/etc. ids the world's internal logic depends on.
    pub fn new_at(
        root: &Path,
        name: String,
        render_distance: i32,
        seed: u32,
        registry: Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
    ) -> Result<Self, WorldError> {
        let dir = root.join("worlds").join(&name);
        std::fs::create_dir_all(&dir)?;
        let tiles_store = TilesStore::open_at(&dir.join("chunks.db"))?;
        let height_map_size = ((render_distance + 2) * 2 * Chunk::SIZE) as usize;
        let height_map = HeightMap::new(height_map_size);
        let generator = Generator::new(seed);
        let grid_size = (2 * (render_distance + 2)) as usize;
        let chunk_grid = ChunkGrid::new(grid_size);
        let pipeline = ChunkPipeline::spawn(
            Arc::clone(&registry),
            base_blocks,
            generator.clone(),
            tiles_store.db_handle(),
            height_map_size,
        );
        Ok(Self {
            name,
            dir,
            tiles_store,
            chunks: slab::Slab::new(),
            by_coord: HashMap::new(),
            chunk_grid,
            height_map,
            generator,
            base_blocks,
            registry,
            block_update_queue: VecDeque::new(),
            player: Player::default(),
            game_time: 0,
            render_distance,
            center_ccoord: Vec3i::new(0, 0, 0),
            pipeline,
            in_flight: HashSet::new(),
            unloaded_chunks: 0,
            updated_blocks: 0,
        })
    }

    /// Convenience wrapper that opens the world relative to the current
    /// working directory (`./worlds/<name>/`). Use [`Self::new_at`] if you
    /// already have an explicit base path.
    pub fn new(
        name: String,
        render_distance: i32,
        seed: u32,
        registry: Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
    ) -> Result<Self, WorldError> {
        let cwd = std::env::current_dir()?;
        Self::new_at(&cwd, name, render_distance, seed, registry, base_blocks)
    }

    #[must_use]
    pub fn name(&self) -> &str {
        &self.name
    }

    #[must_use]
    pub fn chunks(&self) -> &slab::Slab<Chunk> {
        &self.chunks
    }

    #[must_use]
    pub fn block_update_queue(&self) -> &VecDeque<Vec3i> {
        &self.block_update_queue
    }

    #[must_use]
    pub fn player(&self) -> &Player {
        &self.player
    }

    pub fn player_mut(&mut self) -> &mut Player {
        &mut self.player
    }

    #[must_use]
    pub fn game_time(&self) -> u32 {
        self.game_time
    }

    pub fn set_game_time(&mut self, t: u32) {
        self.game_time = t;
    }

    /// Current chunk coord the load window is centered on. Used by `Game` to
    /// detect when the player has crossed a chunk boundary so it can call
    /// [`Self::set_center`] only on transitions instead of every frame.
    #[must_use]
    pub fn center_ccoord(&self) -> Vec3i {
        self.center_ccoord
    }

    #[must_use]
    pub fn render_distance(&self) -> i32 {
        self.render_distance
    }

    /// Block id resolution table the world was constructed with. `BaseBlocks`
    /// is `Copy` so this is a cheap getter.
    #[must_use]
    pub fn base_blocks(&self) -> BaseBlocks {
        self.base_blocks
    }

    /// Hot-path lookup via the grid. Returns `None` if the coord is outside
    /// the grid window or not loaded. **Future:** when [D] lands this returns
    /// `&self.chunks[key].chunk` instead of `&self.chunks[key]`.
    #[must_use]
    pub fn chunk(&self, ccoord: Vec3i) -> Option<&Chunk> {
        let key = self.chunk_grid.get(ccoord)?;
        self.chunks.get(key)
    }

    pub fn chunk_mut(&mut self, ccoord: Vec3i) -> Option<&mut Chunk> {
        let key = self.chunk_grid.get(ccoord)?;
        self.chunks.get_mut(key)
    }

    /// Cold-path lookup via `by_coord`. Covers every loaded chunk, including
    /// those outside the grid window.
    #[must_use]
    pub fn chunk_by_coord(&self, ccoord: Vec3i) -> Option<&Chunk> {
        let key = *self.by_coord.get(&ccoord)?;
        self.chunks.get(key)
    }

    /// Block lookup by world coord, going through the grid where possible.
    #[must_use]
    pub fn block(&self, coord: Vec3i) -> Option<BlockData> {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        Some(self.chunk(cc)?.block(bc, &self.base_blocks))
    }

    /// Block lookup with an air fallback. Mirrors C++ `block_or_air`.
    #[must_use]
    pub fn block_or_air(&self, coord: Vec3i) -> BlockData {
        self.block(coord).unwrap_or(BlockData {
            id: self.base_blocks.air,
            state: crate::blocks::State::default(),
            light: Light::NONE,
        })
    }

    /// Set a block. Marks the chunk dirty + (optionally) queues a block
    /// update. Mirrors C++ `World::put_block`.
    pub fn set_block(&mut self, coord: Vec3i, id: Id, queue_update: bool) {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        let base = self.base_blocks;
        let Some(chunk) = self.chunk_mut(cc) else {
            return;
        };
        chunk.block_mut(bc, &base).id = id;
        if queue_update {
            self.update_block(coord, true);
        }
    }

    /// Solid hitboxes intersecting `box_`. Used by player physics. Port of
    /// C++ `World::hitboxes` — slightly enlarged scan window matches the C++
    /// `lround(box.min) - 2 ..= lround(box.max) + 2` bounds.
    #[must_use]
    pub fn hitboxes(&self, box_: Aabb3d) -> Vec<Aabb3d> {
        let mut res = Vec::new();
        let lo_x = box_.min.x.round() as i32 - 2;
        let hi_x = box_.max.x.round() as i32 + 2;
        let lo_y = box_.min.y.round() as i32 - 2;
        let hi_y = box_.max.y.round() as i32 + 2;
        let lo_z = box_.min.z.round() as i32 - 2;
        let hi_z = box_.max.z.round() as i32 + 2;
        for a in lo_x..=hi_x {
            for b in lo_y..=hi_y {
                for c in lo_z..=hi_z {
                    let coord = Vec3i::new(a, b, c);
                    let id = self.block_or_air(coord).id;
                    let info = self.registry.get(id);
                    if info.solid {
                        let lo = Vec3d::new(f64::from(a), f64::from(b), f64::from(c));
                        let hi = Vec3d::new(
                            f64::from(a + 1),
                            f64::from(b + 1),
                            f64::from(c + 1),
                        );
                        res.push(Aabb3d::new(lo, hi));
                    }
                }
            }
        }
        res
    }

    /// True iff `box_` overlaps any water or lava block. Port of
    /// C++ `World::in_water`.
    #[must_use]
    pub fn in_water(&self, box_: Aabb3d) -> bool {
        let lo_x = box_.min.x.round() as i32 - 1;
        let hi_x = box_.max.x.round() as i32 + 1;
        let lo_y = box_.min.y.round() as i32 - 1;
        let hi_y = box_.max.y.round() as i32 + 1;
        let lo_z = box_.min.z.round() as i32 - 1;
        let hi_z = box_.max.z.round() as i32 + 1;
        for a in lo_x..=hi_x {
            for b in lo_y..=hi_y {
                for c in lo_z..=hi_z {
                    let coord = Vec3i::new(a, b, c);
                    let id = self.block_or_air(coord).id;
                    if id == self.base_blocks.water || id == self.base_blocks.lava {
                        let lo = Vec3d::new(f64::from(a), f64::from(b), f64::from(c));
                        let hi = Vec3d::new(
                            f64::from(a + 1),
                            f64::from(b + 1),
                            f64::from(c + 1),
                        );
                        let block_aabb = Aabb3d::new(lo, hi);
                        if box_.intersects(&block_aabb, 0.0) {
                            return true;
                        }
                    }
                }
            }
        }
        false
    }

    /// Build a tree at `coord`. Direct port of C++ `World::build_tree`. Uses
    /// a deterministic 4-block trunk height so the port is reproducible
    /// without a shared RNG; the C++ original sampled `int(rnd() * 3) + 4`.
    pub fn build_tree(&mut self, coord: Vec3i) {
        let th: i32 = 4;
        let dirt = self.base_blocks.dirt;
        let wood = self.base_blocks.wood;
        let leaf = self.base_blocks.leaf;
        let air = self.base_blocks.air;
        // Trunk
        self.set_block(coord + Vec3i::new(0, -1, 0), dirt, true);
        for yt in 0..th {
            self.set_block(coord + Vec3i::new(0, yt, 0), wood, true);
        }
        // Outer leaves (5×5×2 box around the top of the trunk)
        for xt in 0..5 {
            for zt in 0..5 {
                for yt in 0..2 {
                    let off = Vec3i::new(xt - 2, th - 3 + yt, zt - 2);
                    if self.block_or_air(coord + off).id == air {
                        self.set_block(coord + off, leaf, true);
                    }
                }
            }
        }
        // Inner leaves (3×3×2, skipping the four corners)
        for xt in 0..3 {
            for zt in 0..3 {
                for yt in 0..2 {
                    let off = Vec3i::new(xt - 1, th - 1 + yt, zt - 1);
                    if self.block_or_air(coord + off).id == air
                        && (xt - 1).abs() != (zt - 1).abs()
                    {
                        self.set_block(coord + off, leaf, true);
                    }
                }
            }
        }
        self.set_block(coord + Vec3i::new(0, th, 0), leaf, true);
    }

    /// Spherical-ish destroy. Port of C++ `World::explode`. Uses a simplified
    /// distance-only criterion (the C++ original mixes RNG into the falloff
    /// band; that's left for a later port).
    pub fn explode(&mut self, center: Vec3i, radius: i32) {
        let max_distsqr = radius * radius;
        for fx in (center.x - radius - 1)..=(center.x + radius) {
            for fy in (center.y - radius - 1)..=(center.y + radius) {
                for fz in (center.z - radius - 1)..=(center.z + radius) {
                    let coord = Vec3i::new(fx, fy, fz);
                    let dx = coord.x - center.x;
                    let dy = coord.y - center.y;
                    let dz = coord.z - center.z;
                    let distsqr = dx * dx + dy * dy + dz * dz;
                    if distsqr * 4 <= max_distsqr * 3 {
                        let id = self.block_or_air(coord).id;
                        if !self.registry.get(id).solid {
                            continue;
                        }
                        self.set_block(coord, self.base_blocks.air, true);
                    }
                }
            }
        }
    }

    /// Compute the new lighting / state at `coord` and queue neighbour
    /// updates. Returns true iff the chunk + 6 neighbours are all loaded.
    /// Port of C++ `World::update_block`.
    pub fn update_block(&mut self, coord: Vec3i, initial: bool) -> bool {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        let base = self.base_blocks;

        let curr = match self.chunk(cc) {
            Some(c) => c.block(bc, &base),
            None => return false,
        };

        if curr.id == base.tnt {
            self.explode(coord, 8);
            return true;
        }

        let neighbour_offsets = [
            Vec3i::new(1, 0, 0),
            Vec3i::new(-1, 0, 0),
            Vec3i::new(0, 1, 0),
            Vec3i::new(0, -1, 0),
            Vec3i::new(0, 0, 1),
            Vec3i::new(0, 0, -1),
        ];
        let neighbours: [Option<BlockData>; 6] = [
            self.block(coord + neighbour_offsets[0]),
            self.block(coord + neighbour_offsets[1]),
            self.block(coord + neighbour_offsets[2]),
            self.block(coord + neighbour_offsets[3]),
            self.block(coord + neighbour_offsets[4]),
            self.block(coord + neighbour_offsets[5]),
        ];

        for n in &neighbours {
            if n.is_none() {
                return false;
            }
        }
        let neighbours: [BlockData; 6] = neighbours.map(Option::unwrap);

        let mut sky_light = 0u8;
        let mut block_light = 0u8;
        for n in &neighbours {
            sky_light = sky_light.max(n.light.sky());
            block_light = block_light.max(n.light.block());
        }

        // Top neighbour at SKY_LIGHT propagates skylight without falloff.
        let skylit = coord.y >= 0 && neighbours[2].light.sky() == Light::SKY.sky();

        let curr_solid = self.registry.get(curr.id).solid;
        if curr.id == base.air {
            sky_light = if skylit {
                Light::SKY.sky()
            } else {
                sky_light.saturating_sub(1)
            };
            block_light = block_light.saturating_sub(1);
        } else if !curr_solid {
            sky_light = sky_light.saturating_sub(1);
            block_light = block_light.saturating_sub(1);
        } else {
            sky_light = 0;
            block_light = 0;
        }

        if curr.id == base.glowstone || curr.id == base.lava {
            block_light = 15;
        }

        let new_light = Light::new(sky_light, block_light);
        let mut updated = initial;
        {
            let chunk = self.chunk_mut(cc).expect("chunk verified above");
            let cell = chunk.block_mut(bc, &base);
            if cell.light != new_light {
                cell.light = new_light;
                updated = true;
            }
        }

        if updated {
            for off in &neighbour_offsets {
                self.block_update_queue.push_back(coord + *off);
            }
            // Propagate "neighbour updated" to chunks across each axis edge.
            let size_minus_1 = (Chunk::SIZE - 1) as u32;
            if bc.x == size_minus_1 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(1, 0, 0));
            }
            if bc.x == 0 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(-1, 0, 0));
            }
            if bc.y == size_minus_1 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(0, 1, 0));
            }
            if bc.y == 0 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(0, -1, 0));
            }
            if bc.z == size_minus_1 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(0, 0, 1));
            }
            if bc.z == 0 {
                self.mark_chunk_neighbor_updated(cc + Vec3i::new(0, 0, -1));
            }
        }
        true
    }

    fn mark_chunk_neighbor_updated(&mut self, ccoord: Vec3i) {
        if let Some(c) = self.chunk_mut(ccoord)
            && !c.empty()
        {
            c.mark_neighbor_updated();
        }
    }

    /// Drain up to [`MAX_BLOCK_UPDATES`] from the queue.
    pub fn process_block_updates(&mut self) {
        for _ in 0..MAX_BLOCK_UPDATES {
            let Some(coord) = self.block_update_queue.pop_front() else {
                break;
            };
            self.update_block(coord, false);
            self.updated_blocks = self.updated_blocks.wrapping_add(1);
        }
    }

    /// Slide the chunk grid + height map to centre on `center` (a world
    /// coord). Mirrors C++ `World::set_center`. Note: the height map is
    /// recentered with the same offset the C++ code uses
    /// (`(ccenter - (RD+2)) * SIZE`); the chunk grid origin is the lower
    /// corner of the window in chunk coords.
    pub fn set_center(&mut self, center: Vec3i) {
        let center_chunk = chunk_coord(center);
        self.center_ccoord = center_chunk;
        let half: i32 = self.render_distance + 2;
        let new_origin = center_chunk - Vec3i::new(half, half, half);
        self.chunk_grid.set_center(new_origin);
        // Re-add still-loaded chunks to the grid that were not yet covered.
        for (&coord, &key) in &self.by_coord {
            if self.chunk_grid.contains(coord) {
                self.chunk_grid.set(coord, Some(key));
            }
        }
        self.height_map
            .set_center(new_origin * Chunk::SIZE);
    }

    /// Synchronously load chunks within `render_distance` of the centre and
    /// unload ones outside `render_distance + 1`. Caps at [`MAX_CHUNK_LOADS`]
    /// loads and [`MAX_CHUNK_UNLOADS`] unloads per call.
    pub fn tick_chunk_loading(&mut self) {
        let center = self.center_ccoord;
        let load_dist = self.render_distance;

        // Loads: walk the cube of side `2*load_dist`, sorted by squared
        // distance from centre; insert any cell that isn't loaded yet.
        let mut load_candidates: Vec<(i32, Vec3i)> = Vec::new();
        for dx in -load_dist..=load_dist {
            for dy in -load_dist..=load_dist {
                for dz in -load_dist..=load_dist {
                    let cc = center + Vec3i::new(dx, dy, dz);
                    if self.by_coord.contains_key(&cc) {
                        continue;
                    }
                    let dist = dx * dx + dy * dy + dz * dz;
                    load_candidates.push((dist, cc));
                }
            }
        }
        load_candidates.sort_by_key(|(d, _)| *d);
        load_candidates.truncate(MAX_CHUNK_LOADS);
        for (_, cc) in load_candidates {
            self.load_chunk(cc);
        }

        // Unloads: the unload cutoff in C++ is `RenderDistance + 1`, computed
        // as a per-axis Chebyshev distance.
        let unload_dist = self.render_distance + 1;
        let mut unload_candidates: Vec<(i32, Vec3i)> = Vec::new();
        for &coord in self.by_coord.keys() {
            let d = coord - center;
            if d.x.abs() > unload_dist
                || d.y.abs() > unload_dist
                || d.z.abs() > unload_dist
            {
                let distsqr = d.x * d.x + d.y * d.y + d.z * d.z;
                unload_candidates.push((distsqr, coord));
            }
        }
        unload_candidates.sort_by_key(|(d, _)| std::cmp::Reverse(*d));
        unload_candidates.truncate(MAX_CHUNK_UNLOADS);
        for (_, cc) in unload_candidates {
            self.unload_chunk(cc);
            self.unloaded_chunks = self.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Async sibling of [`Self::tick_chunk_loading`]: walks the load/unload
    /// windows the same way but only **issues requests** to the pipeline
    /// worker; never blocks. Pair with [`Self::poll_load_results`] to drain
    /// finished loads. Mirrors C++ `update_chunk_lists`.
    pub fn tick_chunk_loading_async(&mut self) {
        let center = self.center_ccoord;
        let load_dist = self.render_distance;

        let mut load_candidates: Vec<(i32, Vec3i)> = Vec::new();
        for dx in -load_dist..=load_dist {
            for dy in -load_dist..=load_dist {
                for dz in -load_dist..=load_dist {
                    let cc = center + Vec3i::new(dx, dy, dz);
                    if self.by_coord.contains_key(&cc) || self.in_flight.contains(&cc) {
                        continue;
                    }
                    let dist = dx * dx + dy * dy + dz * dz;
                    load_candidates.push((dist, cc));
                }
            }
        }
        load_candidates.sort_by_key(|(d, _)| *d);
        load_candidates.truncate(MAX_CHUNK_LOADS);
        for (_, cc) in load_candidates {
            if self.pipeline.request_load(cc) {
                self.in_flight.insert(cc);
            }
        }

        // Unloads via the async-save path: the worker writes to sled so
        // unload-on-slide doesn't stall the simulation tick.
        let unload_dist = self.render_distance + 1;
        let mut unload_candidates: Vec<(i32, Vec3i)> = Vec::new();
        for &coord in self.by_coord.keys() {
            let d = coord - center;
            if d.x.abs() > unload_dist
                || d.y.abs() > unload_dist
                || d.z.abs() > unload_dist
            {
                let distsqr = d.x * d.x + d.y * d.y + d.z * d.z;
                unload_candidates.push((distsqr, coord));
            }
        }
        unload_candidates.sort_by_key(|(d, _)| std::cmp::Reverse(*d));
        unload_candidates.truncate(MAX_CHUNK_UNLOADS);
        for (_, cc) in unload_candidates {
            self.unload_chunk_async(cc);
            self.unloaded_chunks = self.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Drain every available [`LoadResult`] from the pipeline worker, install
    /// each into the slab + `by_coord` + `chunk_grid` (re-resolving by coord
    /// per §2.5), and return the list of inserted coords so the caller can
    /// mark them dirty for meshing.
    pub fn poll_load_results(&mut self) -> Vec<Vec3i> {
        let drained = self.pipeline.drain_results();
        let mut inserted = Vec::with_capacity(drained.len());
        for LoadResult { coord, chunk } in drained {
            self.in_flight.remove(&coord);
            // The world may have moved on while the worker was busy. If the
            // coord is no longer needed, drop the result rather than reviving
            // it. We still insert if the coord falls inside `render_distance
            // + 1` (the unload boundary) — exact-window-only would lose
            // chunks that arrived just past a slide.
            let half = self.render_distance + 1;
            let d = coord - self.center_ccoord;
            let in_window =
                d.x.abs() <= half && d.y.abs() <= half && d.z.abs() <= half;
            if !in_window || self.by_coord.contains_key(&coord) {
                continue;
            }
            let key = self.chunks.insert(chunk);
            self.by_coord.insert(coord, key);
            if self.chunk_grid.contains(coord) {
                self.chunk_grid.set(coord, Some(key));
            }
            inserted.push(coord);
        }
        inserted
    }

    /// Fire-and-forget save through the pipeline worker.
    pub fn request_save(&self, coord: Vec3i, bytes: Vec<u8>) {
        self.pipeline.request_save(coord, bytes);
    }

    /// Async-save sibling of [`Self::unload_chunk`]: routes the save through
    /// the pipeline worker instead of blocking on the main-thread sled handle.
    fn unload_chunk_async(&mut self, ccoord: Vec3i) {
        let Some(&key) = self.by_coord.get(&ccoord) else {
            return;
        };
        if let Some(chunk) = self.chunks.get(key)
            && chunk.modified()
        {
            let bytes = chunk.package_to();
            self.pipeline.request_save(ccoord, bytes);
            if let Some(c) = self.chunks.get_mut(key) {
                c.clear_modified();
            }
        }
        if self.chunk_grid.contains(ccoord) {
            self.chunk_grid.set(ccoord, None);
        }
        self.by_coord.remove(&ccoord);
        self.chunks.remove(key);
    }

    /// Insert a chunk into all three structures — the canonical slab, the
    /// `by_coord` map, and the grid (if the coord is in the window).
    fn load_chunk(&mut self, ccoord: Vec3i) {
        if self.by_coord.contains_key(&ccoord) {
            return;
        }
        let mut chunk = Chunk::new(ccoord);
        // Disk first: if the tile store has bytes for this coord, restore
        // them. Otherwise generate. Either way `post_init` runs at the end
        // to mirror the C++ flow.
        let from_disk = match self.tiles_store.load(ccoord) {
            Ok(Some(bytes)) => match chunk.unpackage_from(&bytes) {
                Ok(()) => true,
                Err(_) => false, // bad bytes — fall through to generate
            },
            _ => false,
        };
        if !from_disk {
            chunk.init_generate(&mut self.height_map, &self.generator, &self.base_blocks);
        }
        chunk.post_init();
        let key = self.chunks.insert(chunk);
        self.by_coord.insert(ccoord, key);
        if self.chunk_grid.contains(ccoord) {
            self.chunk_grid.set(ccoord, Some(key));
        }
    }

    /// Remove a chunk from all three structures, saving it to disk first if
    /// modified. Mirrors C++ `_unload_chunk`. Critically: clears the grid
    /// cell **before** freeing the slot, per §2.5 ordering invariant.
    fn unload_chunk(&mut self, ccoord: Vec3i) {
        let Some(&key) = self.by_coord.get(&ccoord) else {
            return;
        };
        // Save first.
        if let Some(chunk) = self.chunks.get(key)
            && chunk.modified()
        {
            let bytes = chunk.package_to();
            let _ = self.tiles_store.save(ccoord, &bytes);
            if let Some(c) = self.chunks.get_mut(key) {
                c.clear_modified();
            }
        }
        // Clear grid before slab — protects against stale-key aliasing.
        if self.chunk_grid.contains(ccoord) {
            self.chunk_grid.set(ccoord, None);
        }
        self.by_coord.remove(&ccoord);
        self.chunks.remove(key);
    }

    /// Save every modified chunk + the player to disk. Uses the world
    /// directory stored at construction time, so this is independent of cwd.
    pub fn save_to_disk(&mut self) -> Result<(), WorldError> {
        // Collect coords first so we can mutate the slab.
        let coords: Vec<Vec3i> = self
            .chunks
            .iter()
            .filter(|(_, c)| c.modified())
            .map(|(_, c)| c.coord())
            .collect();
        for cc in coords {
            if let Some(&key) = self.by_coord.get(&cc) {
                let bytes = self.chunks[key].package_to();
                self.tiles_store.save(cc, &bytes)?;
                self.chunks[key].clear_modified();
            }
        }
        self.tiles_store.flush()?;
        std::fs::create_dir_all(&self.dir)?;
        self.player.save_to(&self.player_path())?;
        Ok(())
    }

    /// Filesystem path of the player save file (`<world_dir>/player.bin`).
    /// Exposed so `Game::new` can attempt a load before the simulation starts.
    #[must_use]
    pub fn player_path(&self) -> PathBuf {
        self.dir.join("player.bin")
    }

    /// Tick the owned [`Player`] against the world's block map. Mirrors the
    /// C++ `player.update(world)`; in Rust the player and the world map are
    /// disjoint fields but the borrow checker can't prove that, so we
    /// momentarily take the player out, run its update against `&*self` as a
    /// `BlockView`, and put it back. The default-sentinel player that lives
    /// in the slot during the call is never observed.
    pub fn update_player(&mut self) {
        let mut player = std::mem::take(&mut self.player);
        player.update(&*self);
        self.player = player;
    }

    /// Enumerate every subdirectory under `<root>/worlds/` and treat each as
    /// a world. Mirrors the C++ `world_menu.cpp` flow, which does
    /// `directory_iterator("worlds")` and shows every directory regardless of
    /// whether it has chunks saved yet — so a freshly-created world (no
    /// `chunks.db`, no `player.bin`) still appears in the list. I/O errors
    /// collapse to an empty result, since the only legitimate failure mode is
    /// a missing `worlds/` dir on first launch.
    #[must_use]
    pub fn list_worlds_at(root: &Path) -> Vec<String> {
        let worlds_dir = root.join("worlds");
        let Ok(entries) = std::fs::read_dir(&worlds_dir) else {
            return Vec::new();
        };
        let mut names: Vec<String> = entries
            .filter_map(Result::ok)
            .filter(|e| e.file_type().is_ok_and(|t| t.is_dir()))
            .filter_map(|e| e.file_name().into_string().ok())
            .collect();
        names.sort();
        names
    }

    /// Recursively delete the world named `name` under `<root>/worlds/`.
    /// Errors propagate to the caller (typically just log them — a missing
    /// directory is a no-op success, while a permission error should surface
    /// to the user).
    pub fn delete_world_at(root: &Path, name: &str) -> Result<(), std::io::Error> {
        let dir = root.join("worlds").join(name);
        if !dir.exists() {
            return Ok(());
        }
        std::fs::remove_dir_all(&dir)
    }
}

// ----------------------------------------------------------------------
//   BlockView impl for World
// ----------------------------------------------------------------------

impl BlockView for World {
    fn block(&self, coord: Vec3i) -> Option<BlockData> {
        World::block(self, coord)
    }

    fn block_or_air(&self, coord: Vec3i) -> BlockData {
        World::block_or_air(self, coord)
    }

    fn hitboxes(&self, box_: Aabb3d) -> Vec<Aabb3d> {
        World::hitboxes(self, box_)
    }

    fn in_water(&self, box_: Aabb3d) -> bool {
        World::in_water(self, box_)
    }
}

// World tests live in `rs/tests/world.rs` (integration). They use
// `World::new_at` with an absolute scratch path so they don't depend on
// cwd, which means cargo can run them in parallel with other integration
// test binaries without a chdir race.

