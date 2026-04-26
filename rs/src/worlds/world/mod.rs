//! World — the canonical owner of every loaded chunk plus the supporting
//! `TilesStore`, async load/save pipeline, block-update queue, and player.
//!
//! Direct port of `src/worlds/worlds.ixx` per `docs/rust_migration.md` §4.6,
//! with one structural simplification: the C++ build's `ChunkPointerArray`
//! sliding window + `ChunkSlot` arena is collapsed here into a plain
//! `HashMap<Vec3i, Chunk>`. Every loaded chunk is one hash-map lookup; there
//! is no separate "loaded core / cold ring" split, and chunk identity is
//! always the integer chunk coord (no opaque slot key in the public API).
//!
//! ## Empty-vs-non-empty invariant
//!
//! `Chunk` allocates its 16³ block array lazily on first write — pure-air
//! chunks (most of the sky) carry no per-cell memory. To keep meshing and
//! rendering O(`#non-empty chunks`) instead of O(`#loaded chunks`), `World`
//! maintains a parallel `non_empty: HashSet<Vec3i>` whose membership tracks
//! `!chunks[coord].empty()` exactly. Every World method that can flip a
//! chunk from empty → non-empty (`set_block`, `update_block`,
//! `poll_load_results`, `load_chunk`) calls [`Self::refresh_non_empty`]
//! after the mutation; the transition is monotonic, so we never have to
//! remove from `non_empty` except on chunk unload.

mod error;
mod pipeline;
mod store;

pub use self::error::WorldError;
pub use self::pipeline::{ChunkPipeline, LoadRequest, LoadResult};
pub use self::store::TilesStore;

use std::collections::{HashMap, HashSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::Arc;

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, Id, Light};
use crate::chunks::Chunk;
use crate::height_maps::HeightMap;
use crate::math::{Aabb3d, Vec3, Vec3d, Vec3i};
use crate::terrain_generation::Generator;
use crate::worlds::player::Player;

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
    /// Every loaded chunk, keyed by integer chunk coord. Empty chunks
    /// (`Chunk::empty() == true`) live here too — being in the map means
    /// "this coord is loaded", not "has block data". See [`Self::non_empty`]
    /// for the parallel set that tracks which entries actually carry data.
    chunks: HashMap<Vec3i, Chunk>,
    /// Subset of `chunks.keys()` whose chunks have allocated block storage.
    /// Invariant: `non_empty.contains(c) ⇔ !chunks[c].empty()`. Maintained
    /// by [`Self::refresh_non_empty`], which every mutator calls after
    /// touching a chunk through the lazy-allocating path
    /// (`Chunk::block_mut`, `unpackage_from`, `init_generate`).
    /// Iterating this set is O(non-empty) — meshing/rendering/save loops
    /// use it to skip pure-air chunks without scanning the full hash map.
    non_empty: HashSet<Vec3i>,
    height_map: HeightMap,
    generator: Generator,
    /// Owned by the `World`; resolved at construction from the registry.
    /// `BaseBlocks` is `Copy` so we don't take an `Arc` for it (per the
    /// migration plan: it's a 19×u16 struct).
    base_blocks: BaseBlocks,
    /// Block registry. Cloned-`Arc` so the world can be torn down without
    /// holding the registry alive past the caller's lifetime.
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
            chunks: HashMap::new(),
            non_empty: HashSet::new(),
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

    // ---- chunk lookup ----

    /// Look up the loaded chunk at `ccoord`, or `None` if it isn't loaded.
    /// Empty chunks count as loaded; the caller can ask `chunk.empty()` to
    /// distinguish.
    #[must_use]
    pub fn chunk(&self, ccoord: Vec3i) -> Option<&Chunk> {
        self.chunks.get(&ccoord)
    }

    /// True iff a chunk is loaded at `ccoord`. Faster + clearer at the call
    /// site than `world.chunk(c).is_some()`.
    #[must_use]
    pub fn is_loaded(&self, ccoord: Vec3i) -> bool {
        self.chunks.contains_key(&ccoord)
    }

    /// Number of loaded chunks (including empty / pure-air ones).
    #[must_use]
    pub fn loaded_count(&self) -> usize {
        self.chunks.len()
    }

    /// Number of loaded chunks that have allocated block data
    /// (`!chunk.empty()`). This is what the renderer / mesher iterate over.
    #[must_use]
    pub fn non_empty_count(&self) -> usize {
        self.non_empty.len()
    }

    /// Iterate over every coord whose chunk is loaded **and** non-empty.
    /// O(non-empty) — the meshing/render/save loops use this so they don't
    /// have to scan empty sky chunks.
    pub fn non_empty_coords(&self) -> impl Iterator<Item = Vec3i> + '_ {
        self.non_empty.iter().copied()
    }

    /// Iterate over `(coord, &Chunk)` pairs for every non-empty chunk. The
    /// chunk lookup goes through the hash map; the `non_empty` invariant
    /// guarantees every coord here is actually loaded.
    pub fn non_empty_chunks(&self) -> impl Iterator<Item = (Vec3i, &Chunk)> + '_ {
        self.non_empty.iter().filter_map(move |coord| {
            self.chunks.get(coord).map(|chunk| (*coord, chunk))
        })
    }

    /// Iterate over every loaded chunk, empty or not. Linear in
    /// [`Self::loaded_count`]; prefer [`Self::non_empty_chunks`] for the
    /// per-frame meshing / render / save passes.
    pub fn loaded_chunks(&self) -> impl Iterator<Item = (Vec3i, &Chunk)> + '_ {
        self.chunks.iter().map(|(c, ch)| (*c, ch))
    }

    // ---- block lookup ----

    /// Block lookup by world coord, going through the loaded chunk if
    /// present. Returns `None` if the coord's chunk isn't loaded.
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
    /// update. Mirrors C++ `World::put_block`. The chunk transitions from
    /// empty → non-empty if this is its first write — `with_chunk_mut`
    /// re-syncs `non_empty` automatically.
    pub fn set_block(&mut self, coord: Vec3i, id: Id, queue_update: bool) {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        let base = self.base_blocks;
        let touched = self
            .with_chunk_mut(cc, |chunk| {
                chunk.block_mut(bc, &base).id = id;
            })
            .is_some();
        if !touched {
            return;
        }
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
        // `with_chunk_mut` re-syncs `non_empty` after the closure returns,
        // covering the empty → non-empty flip that `block_mut` may trigger
        // when this is the chunk's first write.
        self.with_chunk_mut(cc, |chunk| {
            let cell = chunk.block_mut(bc, &base);
            if cell.light != new_light {
                cell.light = new_light;
                updated = true;
            }
        })
        .expect("chunk verified above");

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
        // `mark_neighbor_updated` only flips the `updated` flag; it can
        // never change `empty`. The trip through `with_chunk_mut` keeps
        // every `&mut Chunk` access pinned to the invariant-maintaining
        // path, even though the refresh is a no-op here.
        self.with_chunk_mut(ccoord, |c| {
            if !c.empty() {
                c.mark_neighbor_updated();
            }
        });
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

    /// Slide the load center to `center` (a world coord). Updates the
    /// height-map window so future generation hits a warm cache.
    /// `tick_chunk_loading_async` reads `center_ccoord` to drive the
    /// load/unload windows; this method just updates the centre + cache.
    pub fn set_center(&mut self, center: Vec3i) {
        let center_chunk = chunk_coord(center);
        self.center_ccoord = center_chunk;
        let half: i32 = self.render_distance + 2;
        let new_origin = center_chunk - Vec3i::new(half, half, half);
        self.height_map.set_center(new_origin * Chunk::SIZE);
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
                    if self.chunks.contains_key(&cc) {
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
        for &coord in self.chunks.keys() {
            let d = coord - center;
            if d.x.abs() > unload_dist || d.y.abs() > unload_dist || d.z.abs() > unload_dist {
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
                    if self.chunks.contains_key(&cc) || self.in_flight.contains(&cc) {
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
        // unload-on-slide doesn't stall the simulation tick. Sorted by
        // squared distance descending so the farthest chunks unload first.
        let unload_dist = self.render_distance + 1;
        let mut unload_candidates: Vec<(i32, Vec3i)> = Vec::new();
        for &coord in self.chunks.keys() {
            let d = coord - center;
            if d.x.abs() > unload_dist || d.y.abs() > unload_dist || d.z.abs() > unload_dist {
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
    /// each into the hash map (and refresh `non_empty`), and return the list
    /// of inserted coords so the caller can mark them dirty for meshing.
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
            let in_window = d.x.abs() <= half && d.y.abs() <= half && d.z.abs() <= half;
            if !in_window || self.chunks.contains_key(&coord) {
                continue;
            }
            self.chunks.insert(coord, chunk);
            self.refresh_non_empty(coord);
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
        // The save snapshot is cheap-but-not-free; we want to take it
        // through `with_chunk_mut` so the invariant-maintenance path is
        // honoured even on the unload route. After the closure runs we
        // drop the chunk + mirror the removal in `non_empty`.
        let bytes = self.with_chunk_mut(ccoord, |chunk| {
            if chunk.modified() {
                let bytes = chunk.package_to();
                chunk.clear_modified();
                Some(bytes)
            } else {
                None
            }
        });
        if let Some(Some(bytes)) = bytes {
            self.pipeline.request_save(ccoord, bytes);
        }
        if self.chunks.remove(&ccoord).is_some() {
            self.non_empty.remove(&ccoord);
        }
    }

    /// Insert a chunk: try the on-disk store first, otherwise generate
    /// fresh terrain. The chunk lands in `chunks`; if `init_generate` /
    /// `unpackage_from` allocated the data array, the coord also lands in
    /// `non_empty` via [`Self::refresh_non_empty`].
    fn load_chunk(&mut self, ccoord: Vec3i) {
        if self.chunks.contains_key(&ccoord) {
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
        self.chunks.insert(ccoord, chunk);
        self.refresh_non_empty(ccoord);
    }

    /// Save a chunk to disk if dirty, then drop it from the hash map and
    /// the non-empty set. Mirrors C++ `_unload_chunk`.
    fn unload_chunk(&mut self, ccoord: Vec3i) {
        // `with_chunk_mut` keeps the unload path on the same invariant-
        // maintaining helper as every other `&mut Chunk` consumer; the
        // explicit removals below clean up both maps after the closure.
        let bytes = self.with_chunk_mut(ccoord, |chunk| {
            if chunk.modified() {
                let bytes = chunk.package_to();
                chunk.clear_modified();
                Some(bytes)
            } else {
                None
            }
        });
        if let Some(Some(bytes)) = bytes {
            let _ = self.tiles_store.save(ccoord, &bytes);
        }
        if self.chunks.remove(&ccoord).is_some() {
            self.non_empty.remove(&ccoord);
        }
    }

    /// Reconcile `non_empty` with the actual `Chunk::empty()` state at
    /// `ccoord`. Cheap (one hash-map lookup + one set insert/remove); call
    /// after any path that might transition a chunk's emptiness, which in
    /// practice means anything that goes through `Chunk::block_mut`,
    /// `Chunk::unpackage_from`, or `Chunk::init_generate`.
    ///
    /// Most code shouldn't call this directly — go through
    /// [`Self::with_chunk_mut`] instead, which calls this for you after
    /// every mutable chunk borrow.
    fn refresh_non_empty(&mut self, ccoord: Vec3i) {
        let is_non_empty = self
            .chunks
            .get(&ccoord)
            .is_some_and(|c| !c.empty());
        if is_non_empty {
            self.non_empty.insert(ccoord);
        } else {
            self.non_empty.remove(&ccoord);
        }
    }

    /// Hand a `&mut Chunk` for `ccoord` to `f`, then re-sync the
    /// `non_empty` invariant. Returns `f`'s value, or `None` when no
    /// chunk is loaded at `ccoord`.
    ///
    /// **This is the only sanctioned path to `&mut Chunk` from `World`.**
    /// Going through it guarantees that `non_empty.contains(c) ⇔
    /// !chunks[c].empty()` even if `f` triggers a lazy `Chunk::block_mut`
    /// allocation that would otherwise leave `non_empty` stale. Inserts
    /// (`load_chunk`, `poll_load_results`) call `refresh_non_empty`
    /// directly because they own a fresh `Chunk` rather than a `&mut`;
    /// removals (`unload_chunk`, `unload_chunk_async`) clean up both maps
    /// atomically right after the helper returns.
    fn with_chunk_mut<F, R>(&mut self, ccoord: Vec3i, f: F) -> Option<R>
    where
        F: FnOnce(&mut Chunk) -> R,
    {
        let result = self.chunks.get_mut(&ccoord).map(f)?;
        self.refresh_non_empty(ccoord);
        Some(result)
    }

    /// Save every modified chunk + the player to disk. Uses the world
    /// directory stored at construction time, so this is independent of cwd.
    /// Walks `non_empty` (an empty chunk can't be modified, since
    /// `Chunk::modified` is set by `block_mut` which always allocates).
    pub fn save_to_disk(&mut self) -> Result<(), WorldError> {
        let coords: Vec<Vec3i> = self
            .non_empty
            .iter()
            .copied()
            .filter(|c| {
                self.chunks.get(c).is_some_and(crate::chunks::Chunk::modified)
            })
            .collect();
        for cc in coords {
            // Snapshot + clear-modified through the invariant-maintaining
            // helper. The empty bit can't actually flip here (clear_modified
            // doesn't touch data), but funnelling every `&mut Chunk` through
            // one path keeps the contract trivially auditable.
            let bytes = self
                .with_chunk_mut(cc, |chunk| {
                    let bytes = chunk.package_to();
                    chunk.clear_modified();
                    bytes
                });
            if let Some(bytes) = bytes {
                self.tiles_store.save(cc, &bytes)?;
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
