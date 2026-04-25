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
mod store;

pub use self::error::WorldError;
pub use self::grid::{ChunkGrid, ChunkKey};
pub use self::store::TilesStore;

use std::collections::{HashMap, VecDeque};
use std::path::Path;
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
    pub unloaded_chunks: u32,
    pub updated_blocks: u32,
}

impl World {
    /// Open or create world `name`. `seed` seeds the terrain generator.
    /// `base_blocks` resolves the air/water/dirt/etc. ids the world's
    /// internal logic depends on.
    pub fn new(
        name: String,
        render_distance: i32,
        seed: u32,
        registry: Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
    ) -> Result<Self, WorldError> {
        let tiles_store = TilesStore::open(&name)?;
        let height_map_size = ((render_distance + 2) * 2 * Chunk::SIZE) as usize;
        let height_map = HeightMap::new(height_map_size);
        let generator = Generator::new(seed);
        let grid_size = (2 * (render_distance + 2)) as usize;
        let chunk_grid = ChunkGrid::new(grid_size);
        Ok(Self {
            name,
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
            unloaded_chunks: 0,
            updated_blocks: 0,
        })
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

    /// Save every modified chunk + the player to disk.
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
        let world_dir = Path::new("worlds").join(&self.name);
        std::fs::create_dir_all(&world_dir)?;
        self.player.save_to(&world_dir.join("player.bin"))?;
        Ok(())
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

// ======================================================================
//   Tests
// ======================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{register_base_blocks, BlockRegistry};
    use crate::math::Vec3d;
    use std::path::PathBuf;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::sync::Arc;

    /// Test scratch directory rooted in the OS temp dir. Mirrors
    /// `i18n.rs::tests::ScratchDir` so we don't need a `tempfile` dep. Each
    /// instance gets a unique subdir; `Drop` does best-effort cleanup.
    struct ScratchDir {
        path: PathBuf,
        prev_cwd: PathBuf,
    }

    impl ScratchDir {
        fn new(tag: &str) -> Self {
            static COUNTER: AtomicU64 = AtomicU64::new(0);
            let n = COUNTER.fetch_add(1, Ordering::Relaxed);
            let pid = std::process::id();
            let path = std::env::temp_dir().join(format!("neworld-world-{tag}-{pid}-{n}"));
            let _ = std::fs::remove_dir_all(&path);
            std::fs::create_dir_all(&path).expect("create scratch dir");
            let prev_cwd = std::env::current_dir().expect("cwd");
            // World::new opens "worlds/<name>/chunks.db" as a relative path,
            // so chdir into the scratch dir for the duration of the test.
            std::env::set_current_dir(&path).expect("chdir into scratch");
            Self { path, prev_cwd }
        }
    }

    impl Drop for ScratchDir {
        fn drop(&mut self) {
            // Restore cwd before nuking the scratch dir.
            let _ = std::env::set_current_dir(&self.prev_cwd);
            let _ = std::fs::remove_dir_all(&self.path);
        }
    }

    /// World tests run in a single process; sled rejects concurrent opens of
    /// the same DB and we change cwd, so serialise everything that touches
    /// `ScratchDir` behind a global mutex.
    use std::sync::Mutex;
    static TEST_LOCK: Mutex<()> = Mutex::new(());

    fn make_registry() -> (Arc<BlockRegistry>, BaseBlocks) {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        (Arc::new(r), base)
    }

    fn build_world(name: &str, render_distance: i32) -> World {
        let (registry, base) = make_registry();
        World::new(name.to_owned(), render_distance, 0, registry, base)
            .expect("world::new")
    }

    // ChunkGrid tests live in `world/grid.rs`; TilesStore tests in `world/store.rs`.

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
        // The chunk should show as modified.
        let cc = chunk_coord(coord);
        let chunk = w.chunk(cc).expect("loaded chunk");
        assert!(chunk.modified());
    }

    #[test]
    fn world_block_or_air_returns_air_for_unloaded_coord() {
        let _guard = TEST_LOCK.lock().unwrap();
        let _scratch = ScratchDir::new("air");
        let w = build_world("air", 1);
        // No tick_chunk_loading: nothing is loaded yet.
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
        // Pick a coord inside the load window.
        let cc = Vec3i::new(0, 0, 0);
        let by_grid = w.chunk(cc).expect("via grid");
        let by_map = w.chunk_by_coord(cc).expect("via map");
        // Both should refer to a chunk at the same coord.
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

        // Slide the grid far enough that `cc` falls outside the new window
        // but no unloads have run yet.
        let far = Vec3i::new(10_000, 0, 10_000);
        w.set_center(far * Chunk::SIZE);
        // After the slide, cc is no longer in the grid window.
        assert!(w.chunk(cc).is_none());
        // But `by_coord` still has it (no unload yet).
        assert!(w.chunk_by_coord(cc).is_some());
    }

    #[test]
    fn world_update_block_skips_when_neighbours_unloaded() {
        let _guard = TEST_LOCK.lock().unwrap();
        let _scratch = ScratchDir::new("update-skip");
        let mut w = build_world("update-skip", 1);
        // Load only the centre chunk by manually inserting it.
        w.set_center(Vec3i::new(0, 0, 0));
        // We don't call tick_chunk_loading; instead load just one chunk.
        w.load_chunk(Vec3i::new(0, 0, 0));
        // A coord inside the loaded chunk, but on the +x face, so the +x
        // neighbour chunk (1,0,0) is unloaded.
        let coord = Vec3i::new(15, 5, 5);
        // Returns false because some neighbours aren't loaded.
        assert!(!w.update_block(coord, true));
        // Queue should be empty.
        assert!(w.block_update_queue().is_empty());
    }

    #[test]
    fn world_update_block_queues_neighbour_updates_when_all_loaded() {
        let _guard = TEST_LOCK.lock().unwrap();
        let _scratch = ScratchDir::new("update-queue");
        let mut w = build_world("update-queue", 1);
        w.set_center(Vec3i::new(0, 0, 0));
        w.tick_chunk_loading();
        // A coord with all 6 neighbours in loaded chunks.
        let coord = Vec3i::new(2, 3, 4);
        let drained_before = w.block_update_queue().len();
        let ok = w.update_block(coord, true);
        assert!(ok, "neighbours should be loaded");
        // Six neighbour offsets pushed onto the queue.
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
        // Ensure by_coord and slab agree on cardinality.
        assert_eq!(w.by_coord.len(), w.chunks.len());
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
        let aabb = Aabb3d::new(
            Vec3d::new(0.0, 0.0, 0.0),
            Vec3d::new(1.0, 1.0, 1.0),
        );
        let inherent = World::hitboxes(&w, aabb);
        let via_trait = <World as BlockView>::hitboxes(&w, aabb);
        assert_eq!(inherent.len(), via_trait.len());
        let inherent = World::in_water(&w, aabb);
        let via_trait = <World as BlockView>::in_water(&w, aabb);
        assert_eq!(inherent, via_trait);
    }
}
