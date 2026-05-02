//! World — the canonical owner of every loaded chunk plus the supporting
//! `TilesStore`, async load/save pipeline, block-update queue, and player.
//!
//! Direct port of `src/worlds/worlds.ixx` per `docs/rust_migration.md` §4.6,
//! with one structural simplification: the C++ build's `ChunkPointerArray`
//! sliding window + `ChunkSlot` arena is collapsed here into a plain
//! `HashMap<Vec3i, Chunk>`. Every loaded chunk is one hash-map lookup; there
//! is no separate "loaded core / cold ring" split, and chunk identity is
//! always the integer chunk coord (no opaque slot key in the public API).

use super::error::WorldError;
use super::metadata::WorldMetadata;
use super::pipeline::{ChunkPipeline, LoadResult};
use super::store::TilesStore;

use std::cmp::{Ordering, Reverse};
use std::collections::{BinaryHeap, HashMap, HashSet, VecDeque};
use std::path::{Path, PathBuf};
use std::sync::Arc;

use crate::blocks::{BaseBlocks, BlockData, BlockId, BlockRegistry, Id, Light, State};
use super::chunk::Chunk;
use crate::height_maps::HeightMap;
use crate::math::{Aabbd, Vec3d, Vec3i, Vec3u};
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

/// Extra radius (in chunks) loaded beyond `render_distance` so every
/// meshable chunk has all six axis neighbours present. The mesher samples a
/// 1-cell padded neighbourhood; without this buffer, chunks at the render
/// boundary would mesh against `block_or_air()` air defaults across the
/// missing neighbour seam, leaving visible cracks. Unload happens further
/// out than `render_distance + LOAD_RADIUS_BUFFER` so a chunk doesn't
/// oscillate between loaded and unloaded around the boundary.
pub const LOAD_RADIUS_BUFFER: i32 = 1;

/// Cells sampled per non-empty chunk on each random-tick *fire*. Three
/// matches the order of magnitude of vanilla MC's `random-tick speed = 3`
/// default — slow enough that grass spread is gradual but visible.
pub const RANDOM_TICKS_PER_CHUNK: usize = 3;

/// Sim ticks between random-tick fires. With `tick_sim` running at 30 Hz,
/// 30 means "fire once per second". The chunk loop iterates every loaded
/// non-empty chunk, so capping the cadence keeps the cost bounded at high
/// render distance — at rd=32 the loaded set is ~40K chunks, of which
/// ~10K are non-empty; firing every tick would make this loop a real
/// per-frame cost.
pub const RANDOM_TICK_PERIOD: u32 = 30;

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
pub fn block_coord(coord: Vec3i) -> Vec3u {
    let mask = (Chunk::SIZE - 1) as u32;
    Vec3u::new(
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
    fn hitboxes(&self, box_: Aabbd) -> Vec<Aabbd>;
    fn in_water(&self, box_: Aabbd) -> bool;
}

// ----------------------------------------------------------------------
//   ByDist — heap entry that orders solely on a squared-distance i32
// ----------------------------------------------------------------------

/// Heap entry pairing a squared distance with a chunk coord. `Vec3i`
/// (`cgmath::Vector3<i32>`) doesn't implement `Ord`, so the candidate
/// loops can't put `(i32, Vec3i)` directly into a `BinaryHeap`. The
/// manual `Ord` here orders only on `dist`; ties are resolved
/// arbitrarily but consistently.
///
/// `pub(crate)` so `Game::pump_meshing` can use the same bounded-heap
/// pattern when picking nearest dirty chunks.
#[derive(Clone, Copy, PartialEq, Eq)]
pub(crate) struct ByDist {
    pub(crate) dist: i32,
    pub(crate) coord: Vec3i,
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
//   LoadedCore — concentric "known fully loaded" region
// ----------------------------------------------------------------------

/// Tracks a centered cube of chunks that is guaranteed to be fully
/// loaded (or in flight, for the async pipeline). Lets the load-window
/// scan in [`World::collect_load_candidates`] skip the inner shells —
/// only the boundary `(radius, render_distance + LOAD_RADIUS_BUFFER]`
/// needs scanning per call. Direct port of C++ `LoadedCore` in
/// `worlds.ixx`.
///
/// **Invariant.** For every chunk coord `c` with chebyshev distance
/// `max(|c.x - ccenter.x|, |c.y - ccenter.y|, |c.z - ccenter.z|) <=
/// radius`, either `chunks.contains_key(&c)` or `in_flight.contains(&c)`.
/// `radius == -1` means "nothing known"; this is the initial state.
///
/// **Maintenance.**
/// - On player movement (`ccenter` shifts by chebyshev distance `m`),
///   shrink to `max(-1, radius - m)`. The cells still inside the new
///   ball are a subset of the old ball, so the invariant survives.
/// - On chunk unload at `ccoord` with chebyshev distance `d` from
///   `ccenter`, shrink to `min(radius, d - 1)`. The unloaded cell
///   breaks the promise at radius `d`; everything strictly inside is
///   still loaded.
/// - On a shell scan that finds zero missing cells, advance `radius`
///   to that shell. Any deeper shell that *also* finds zero would
///   advance further, but the moment a shell yields a candidate we
///   stop advancing — the deeper invariant is broken.
#[derive(Clone, Copy)]
struct LoadedCore {
    ccenter: Vec3i,
    radius: i32,
}

impl Default for LoadedCore {
    fn default() -> Self {
        Self {
            ccenter: Vec3i::new(0, 0, 0),
            radius: -1,
        }
    }
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
    /// Per-world canonical block-id ↔ name mapping. Loaded from
    /// `<dir>/world.dat` if the file exists (verifying magic + version) or
    /// snapshotted from the current registry on first run. Saved back on
    /// every [`Self::save_to_disk`] call so the next load can reconstruct
    /// the translation table.
    world_metadata: WorldMetadata,
    /// Translation table applied when *loading* a chunk: indexed by the
    /// canonical id stored on disk, gives the `BlockId` to substitute for
    /// the in-memory cell. Empty `Vec` means identity. Computed once at
    /// `new_at` time from `world_metadata` + `registry`.
    chunk_load_table: Vec<BlockId>,
    /// Translation table applied when *saving* a chunk: indexed by the
    /// in-memory current registry id, gives the canonical id (`u16`) to
    /// write. Empty `Vec` means identity.
    chunk_save_table: Vec<u16>,
    /// Every loaded chunk, keyed by integer chunk coord. Empty chunks
    /// (`Chunk::empty() == true`) live here too — being in the map means
    /// "this coord is loaded", not "has block data". Loops that only need
    /// chunks with allocated storage filter on `!c.empty()`.
    chunks: HashMap<Vec3i, Chunk>,
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
    /// Async load/save pipeline (F5). One worker thread, `crossbeam-channel`
    /// transport. See `pipeline.rs`.
    pipeline: ChunkPipeline,
    /// Coords currently in flight on the pipeline. Prevents double-issuing
    /// loads for the same coord while the worker is busy with it.
    in_flight: HashSet<Vec3i>,
    /// "Known fully loaded" centered region — see [`LoadedCore`]. Doubles
    /// as the canonical "current load-window centre" (its `ccenter`
    /// supersedes a separate `center_ccoord` field). Lets the per-tick
    /// load scan skip the bulk of the cube once the inner shells have
    /// settled. Big win at high `render_distance`, where the full cube
    /// is `(2·rd+3)³` cells.
    loaded_core: LoadedCore,
    /// Tiny LCG state for `random_tick`. Avoids pulling `rand` into `World`
    /// for what amounts to "pick three cells per non-empty chunk per
    /// tick" — same approach `Game` takes for break-particle jitter.
    rng: u64,
    /// Counter incremented on every `random_tick` call. The body runs
    /// only when `phase % RANDOM_TICK_PERIOD == 0`; intermediate calls
    /// are O(1) early-returns. See [`RANDOM_TICK_PERIOD`].
    random_tick_phase: u32,
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

        // Load `world.dat` if present; otherwise snapshot the current
        // registry as the canonical mapping for this world. The empty-`Vec`
        // representation is used as the identity-translation fast path.
        let metadata_path = dir.join("world.dat");
        let (world_metadata, chunk_load_table, chunk_save_table) = if metadata_path.exists() {
            let meta = WorldMetadata::load_from(&metadata_path)?;
            let load = meta.canonical_to_current(&registry);
            let save = meta.current_to_canonical(&registry);
            let load_identity = load.len() == registry.len()
                && load.iter().enumerate().all(|(i, id)| id.0 as usize == i);
            let save_identity = save.len() == registry.len()
                && save.iter().enumerate().all(|(i, &c)| c as usize == i);
            (
                meta,
                if load_identity { Vec::new() } else { load },
                if save_identity { Vec::new() } else { save },
            )
        } else {
            // Fresh world: canonical mapping == current registry ⇒ identity.
            (WorldMetadata::from_registry(&registry), Vec::new(), Vec::new())
        };

        let height_map_size = ((render_distance + 2) * 2 * Chunk::SIZE) as usize;
        let height_map = HeightMap::new(height_map_size);
        let generator = Generator::new(seed);
        let pipeline = ChunkPipeline::spawn(
            Arc::clone(&registry),
            base_blocks,
            generator.clone(),
            tiles_store.db_handle(),
            height_map_size,
            Arc::new(chunk_load_table.clone()),
        );
        Ok(Self {
            name,
            dir,
            tiles_store,
            world_metadata,
            chunk_load_table,
            chunk_save_table,
            chunks: HashMap::new(),
            height_map,
            generator,
            base_blocks,
            registry,
            block_update_queue: VecDeque::new(),
            player: Player::default(),
            game_time: 0,
            render_distance,
            pipeline,
            in_flight: HashSet::new(),
            loaded_core: LoadedCore::default(),
            // Mix the seed into a non-zero LCG state so `random_tick`'s
            // initial pulls aren't trivially predictable from one world
            // creation to the next.
            rng: 0x9E37_79B9_7F4A_7C15_u64.wrapping_add(u64::from(seed)),
            random_tick_phase: 0,
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

    /// Advance the in-game clock by `ticks`. Wraps at [`Self::DAY_TICKS`]
    /// so the clock remains comparable across a long-running world. Called
    /// once per sim tick from [`crate::game::Game::tick_sim`] (with an
    /// extra multiplier while F8 is held).
    pub fn advance_game_time(&mut self, ticks: u32) {
        self.game_time = self.game_time.wrapping_add(ticks) % Self::DAY_TICKS;
    }

    /// Length of one in-game day in sim ticks. 30 ticks/sec × 60 s × 20 min
    /// → 36000. Sun-direction and sky-light multiplier wrap on this period.
    pub const DAY_TICKS: u32 = 30 * 60 * 20;

    /// Current chunk coord the load window is centered on. Used by `Game` to
    /// detect when the player has crossed a chunk boundary so it can call
    /// [`Self::set_center`] only on transitions instead of every frame.
    /// Reads through to the loaded-core centre — they're the same point.
    #[must_use]
    pub fn center_ccoord(&self) -> Vec3i {
        self.loaded_core.ccenter
    }

    #[must_use]
    pub fn render_distance(&self) -> i32 {
        self.render_distance
    }

    /// Update the render distance live. Resizes the height-map sliding cache
    /// and re-pivots it around the current centre so the next
    /// `tick_chunk_loading_async` issues loads / unloads against the new
    /// window. Chunks that fall outside the shrunk window will be unloaded
    /// over the next few ticks; chunks inside an expanded window will stream
    /// in on the same path. No-op if the value is unchanged.
    pub fn set_render_distance(&mut self, distance: i32) {
        let distance = distance.max(1);
        if self.render_distance == distance {
            return;
        }
        self.render_distance = distance;
        // Re-pivot the height-map cache to the new size around the existing
        // centre. `set_center` reads `self.render_distance` to pick the new
        // origin, so we just call through to the same code path the
        // boundary-cross handler uses.
        let center_world = self.loaded_core.ccenter * Chunk::SIZE;
        // height_map needs to be rebuilt at the new size. `set_center` only
        // shifts the existing window; we drop and rebuild here so the cache
        // matches the new diameter exactly.
        let new_size = ((distance + 2) * 2 * Chunk::SIZE) as usize;
        self.height_map = crate::height_maps::HeightMap::new(new_size);
        self.set_center(center_world);
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

    /// True iff every one of `ccoord`'s 26 neighbour chunks (the full
    /// 3×3×3 cube minus self) is loaded.
    ///
    /// The mesher's 18×18×18 padded snapshot reads cells from every
    /// neighbour, including the 12 edge-diagonal and 8 corner-diagonal
    /// chunks: smooth-lighting's 4-cell AO tap at a chunk-corner face
    /// pulls from the diagonally-adjacent chunk (e.g. the +X face at
    /// chunk-corner cell `(15, 0, 15)` averages neighbours including
    /// `(+1, -1, +1)`, which lives in the corner-diagonal chunk). A
    /// face-adjacent-only check would leave those reads pulling stale
    /// air, producing a darkened seam at every chunk corner.
    ///
    /// The load radius ([`LOAD_RADIUS_BUFFER`]) keeps every chunk
    /// inside the render window neighbour-complete, so this is a tight
    /// filter rather than a hot gate, but it stays as a defensive
    /// check for spawn-frame pop-in and fast-teleport edge cases.
    #[must_use]
    pub fn has_neighbours_loaded(&self, ccoord: Vec3i) -> bool {
        for dz in -1..=1 {
            for dy in -1..=1 {
                for dx in -1..=1 {
                    if dx == 0 && dy == 0 && dz == 0 {
                        continue;
                    }
                    if !self.is_loaded(ccoord + Vec3i::new(dx, dy, dz)) {
                        return false;
                    }
                }
            }
        }
        true
    }

    /// Number of loaded chunks (including empty / pure-air ones).
    #[must_use]
    pub fn loaded_count(&self) -> usize {
        self.chunks.len()
    }

    /// Iterate over every loaded chunk, empty or not. Linear in
    /// [`Self::loaded_count`]; callers that only care about chunks with
    /// allocated storage filter on `!c.empty()`.
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
    /// update. Mirrors C++ `World::put_block` exactly: write the new id,
    /// then run the per-cell relaxation via `update_block`. Light removal
    /// is *not* run synchronously — the relaxation pass converges over
    /// the next several sim ticks via [`Self::process_block_updates`].
    pub fn set_block(&mut self, coord: Vec3i, id: Id, queue_update: bool) {
        self.set_block_with_state(coord, id, State::default(), queue_update);
    }

    /// Like [`Self::set_block`] but also writes a state byte. Used when
    /// placing orientation-bearing blocks (logs, etc.) so the placement
    /// face/direction lands in the cell's state immediately. Existing
    /// callers that don't care about state should keep using
    /// [`Self::set_block`], which forwards here with `State::default()`.
    pub fn set_block_with_state(&mut self, coord: Vec3i, id: Id, state: State, queue_update: bool) {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        let base = self.base_blocks;
        let touched = self
            .with_chunk_mut(cc, |chunk| {
                let cell = chunk.block_mut(bc, &base);
                cell.id = id;
                cell.state = state;
            })
            .is_some();
        if !touched {
            return;
        }
        // `block_mut` flipped `cc`'s `updated` flag. Flip the other parent
        // chunks of the 26 block-neighbours of `coord` too, so every chunk
        // whose padded mesher region samples this cell will re-mesh.
        self.mark_block_neighbour_chunks_updated(coord);
        if queue_update {
            self.update_block(coord, true);
        }
    }

    /// Solid hitboxes intersecting `box_`. Used by player physics. Port of
    /// C++ `World::hitboxes` — slightly enlarged scan window matches the C++
    /// `lround(box.min) - 2 ..= lround(box.max) + 2` bounds.
    ///
    /// Unloaded chunks that overlap the scan window contribute one big
    /// chunk-sized hitbox each, treated as fully solid. Without this
    /// guard, a fast-falling player whose simulation outruns the async
    /// chunk loader would tunnel through the chunk-aligned grid into
    /// the void; the chunk-sized hitbox keeps physics safe until the
    /// real chunk lands and the per-cell probe takes over.
    #[must_use]
    pub fn hitboxes(&self, box_: Aabbd) -> Vec<Aabbd> {
        let mut res = Vec::new();
        let lo_x = box_.min.x.round() as i32 - 2;
        let hi_x = box_.max.x.round() as i32 + 2;
        let lo_y = box_.min.y.round() as i32 - 2;
        let hi_y = box_.max.y.round() as i32 + 2;
        let lo_z = box_.min.z.round() as i32 - 2;
        let hi_z = box_.max.z.round() as i32 + 2;

        // First pass — scan every chunk the AABB overlaps. For each
        // unloaded chunk, emit a single chunk-sized hitbox; for each
        // loaded chunk, fall back to per-cell probing inside it.
        let cc_lo = chunk_coord(Vec3i::new(lo_x, lo_y, lo_z));
        let cc_hi = chunk_coord(Vec3i::new(hi_x, hi_y, hi_z));
        let s = Chunk::SIZE;
        for cz in cc_lo.z..=cc_hi.z {
            for cy in cc_lo.y..=cc_hi.y {
                for cx in cc_lo.x..=cc_hi.x {
                    let ccoord = Vec3i::new(cx, cy, cz);
                    if self.is_loaded(ccoord) {
                        continue;
                    }
                    // Emit one solid chunk-sized AABB for the unloaded
                    // chunk. The per-cell loop below skips this chunk's
                    // cells entirely, so we don't double-emit.
                    let origin_x = cx * s;
                    let origin_y = cy * s;
                    let origin_z = cz * s;
                    let lo = Vec3d::new(
                        f64::from(origin_x),
                        f64::from(origin_y),
                        f64::from(origin_z),
                    );
                    let hi = Vec3d::new(
                        f64::from(origin_x + s),
                        f64::from(origin_y + s),
                        f64::from(origin_z + s),
                    );
                    res.push(Aabbd::new(lo, hi));
                }
            }
        }

        for a in lo_x..=hi_x {
            for b in lo_y..=hi_y {
                for c in lo_z..=hi_z {
                    let coord = Vec3i::new(a, b, c);
                    // Skip cells that belong to an unloaded chunk —
                    // they're already covered by the chunk-sized
                    // hitbox above.
                    if !self.is_loaded(chunk_coord(coord)) {
                        continue;
                    }
                    let id = self.block_or_air(coord).id;
                    let info = self.registry.get(id);
                    if info.solid {
                        let lo = Vec3d::new(f64::from(a), f64::from(b), f64::from(c));
                        let hi = Vec3d::new(f64::from(a + 1), f64::from(b + 1), f64::from(c + 1));
                        res.push(Aabbd::new(lo, hi));
                    }
                }
            }
        }
        res
    }

    /// True iff `box_` overlaps any water or lava block. Port of
    /// C++ `World::in_water`.
    #[must_use]
    pub fn in_water(&self, box_: Aabbd) -> bool {
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
                        let hi = Vec3d::new(f64::from(a + 1), f64::from(b + 1), f64::from(c + 1));
                        let block_aabb = Aabbd::new(lo, hi);
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
                    if self.block_or_air(coord + off).id == air && (xt - 1).abs() != (zt - 1).abs()
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

        // Light-passthrough classification:
        // * `!opaque && !translucent` → air-like (air, glass, leaf): full
        //   skylight column passthrough plus a `-1` falloff for diffuse
        //   light. Glass and leaf join air here so a window or canopy
        //   doesn't black out the cells below it.
        // * `!opaque && translucent` → water-like (water, lava, ice):
        //   `-1` falloff with no skylight fast-path — water in
        //   particular dims sunlight as you go deeper, which the
        //   current behaviour already encoded for non-solid translucent
        //   blocks; we preserve that and extend it to ice.
        // * `opaque` → block all light.
        let curr_info = self.registry.get(curr.id);
        let curr_opaque = curr_info.opaque;
        let curr_translucent = curr_info.translucent;
        if !curr_opaque && !curr_translucent {
            sky_light = if skylit {
                Light::SKY.sky()
            } else {
                sky_light.saturating_sub(1)
            };
            block_light = block_light.saturating_sub(1);
        } else if !curr_opaque {
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
            // Propagate "needs re-mesh" to every chunk whose padded
            // region samples this cell — full 3×3×3 cube of parent
            // chunks, not just the 6 axis-faces, since smooth-light AO
            // taps reach diagonal neighbours.
            self.mark_block_neighbour_chunks_updated(coord);
        }
        true
    }

    /// Mark every chunk whose padded mesher region samples block `coord`
    /// as needing a re-mesh. The mesher's 18×18×18 padded snapshot reads
    /// the 26 block-neighbours of each cell, so a write at `coord`
    /// invalidates the meshes of every chunk that contains one of those
    /// neighbours — at most 8 distinct chunks (when `coord` is a
    /// chunk-corner cell), 1 when `coord` is interior.
    ///
    /// The cell's own chunk is intentionally NOT touched here: `block_mut`
    /// already flips its `updated` flag during the write that called us.
    /// We only mark the (up to 7) other chunks that need to refresh.
    fn mark_block_neighbour_chunks_updated(&mut self, coord: Vec3i) {
        let cc = chunk_coord(coord);
        let bc = block_coord(coord);
        let last = (Chunk::SIZE - 1) as u32;

        // Per-axis chunk-coord offsets that need marking. A cell at the
        // 0-edge of its chunk has block-neighbours in the (-1) chunk; at
        // the (SIZE - 1)-edge, in the (+1) chunk. Interior cells contribute
        // only the cell's own chunk (offset 0), which we skip below.
        let xs: &[i32] = if bc.x == 0 {
            &[-1, 0]
        } else if bc.x == last {
            &[0, 1]
        } else {
            &[0]
        };
        let ys: &[i32] = if bc.y == 0 {
            &[-1, 0]
        } else if bc.y == last {
            &[0, 1]
        } else {
            &[0]
        };
        let zs: &[i32] = if bc.z == 0 {
            &[-1, 0]
        } else if bc.z == last {
            &[0, 1]
        } else {
            &[0]
        };

        for &dx in xs {
            for &dy in ys {
                for &dz in zs {
                    let target = cc + Vec3i::new(dx, dy, dz);
                    self.with_chunk_mut(target, |c| {
                        if !c.empty() {
                            c.mark_neighbor_updated();
                        }
                    });
                }
            }
        }
    }

    /// Mark every chunk in the 3×3×3 cube around `ccoord` as needing a
    /// re-mesh. Used after a chunk's data arrives — terrain generation
    /// / disk load — so neighbouring chunks whose padded-border samples
    /// used to read unloaded-air placeholders pick up the real cells
    /// and re-mesh seamlessly.
    fn mark_chunk_neighbour_chunks_updated(&mut self, ccoord: Vec3i) {
        for dz in -1..=1 {
            for dy in -1..=1 {
                for dx in -1..=1 {
                    let target = ccoord + Vec3i::new(dx, dy, dz);
                    self.with_chunk_mut(target, |c| {
                        if !c.empty() {
                            c.mark_neighbor_updated();
                        }
                    });
                }
            }
        }
    }

    /// Probabilistic per-chunk cell sampler — the "random tick" hook that
    /// drives slow-cycle world updates (grass spread / smother / future
    /// fluid creep). For each non-empty chunk, picks
    /// [`RANDOM_TICKS_PER_CHUNK`] random cells and feeds each through
    /// [`Self::random_tick_block`].
    ///
    /// Throttled to fire once every [`RANDOM_TICK_PERIOD`] sim ticks
    /// (i.e. once per second at 30 Hz). Calls in between bump the phase
    /// counter and return immediately. Throttling matters at high render
    /// distance — the chunk-iteration loop is O(non-empty chunks loaded),
    /// which can be tens of thousands.
    pub fn random_tick(&mut self) {
        let phase = self.random_tick_phase;
        self.random_tick_phase = self.random_tick_phase.wrapping_add(1);
        if !phase.is_multiple_of(RANDOM_TICK_PERIOD) {
            return;
        }

        // Snapshot coords first so we don't borrow `self.chunks` while
        // mutating it via `set_block`.
        let coords: Vec<Vec3i> = self
            .chunks
            .iter()
            .filter(|(_, c)| !c.empty())
            .map(|(c, _)| *c)
            .collect();
        for cc in coords {
            for _ in 0..RANDOM_TICKS_PER_CHUNK {
                let r = self.rand_u32();
                let bx = (r & 0xF) as i32;
                let by = ((r >> 4) & 0xF) as i32;
                let bz = ((r >> 8) & 0xF) as i32;
                let world_coord = cc * Chunk::SIZE + Vec3i::new(bx, by, bz);
                self.random_tick_block(world_coord);
            }
        }
    }

    /// Inspect one cell and apply random-tick rules. Currently:
    /// * **Grass smother:** a `grass` cell with an opaque block directly
    ///   above flips back to `dirt` (sun starves).
    /// * **Grass spread:** a `dirt` cell with no opaque block above and at
    ///   least one of the 4 horizontal neighbours being `grass` flips to
    ///   `grass`.
    ///
    /// Air / water / unrelated cells are no-ops, so this is safe to call
    /// on any coord (including unloaded ones — `block` returns `None`).
    fn random_tick_block(&mut self, coord: Vec3i) {
        let Some(cell) = self.block(coord) else {
            return;
        };
        let base = self.base_blocks;

        if cell.id == base.grass {
            // Smother: opaque block directly above shadows the grass.
            let above = self.block_or_air(coord + Vec3i::new(0, 1, 0));
            if self.registry.get(above.id).opaque {
                self.set_block(coord, base.dirt, true);
            }
            return;
        }

        if cell.id == base.dirt {
            // Spread: needs at least one horizontal grass neighbour and a
            // non-opaque cell above.
            let above = self.block_or_air(coord + Vec3i::new(0, 1, 0));
            if self.registry.get(above.id).opaque {
                return;
            }
            let horizontal = [
                Vec3i::new(1, 0, 0),
                Vec3i::new(-1, 0, 0),
                Vec3i::new(0, 0, 1),
                Vec3i::new(0, 0, -1),
            ];
            for off in horizontal {
                if self.block_or_air(coord + off).id == base.grass {
                    self.set_block(coord, base.grass, true);
                    return;
                }
            }
        }
    }

    /// Tiny LCG step → `u32`. Numerical Recipes constants on a `u64`
    /// state, taking the high 32 bits. `World::random_tick` uses this
    /// to jitter cell selection without pulling in the `rand` crate.
    fn rand_u32(&mut self) -> u32 {
        self.rng = self
            .rng
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1_442_695_040_888_963_407);
        (self.rng >> 32) as u32
    }

    /// Lazy iterator over every loaded chunk whose `Chunk::updated()`
    /// flag is set. Does **not** clear the flag — callers (`pump_meshing`)
    /// pick a bounded subset and explicitly call
    /// [`Self::clear_updated_chunks`] for the ones they actually rebuilt.
    /// Chunks the caller skips this frame stay marked and re-appear in the
    /// next iterator pass.
    ///
    /// Walking is O(loaded chunks), but no allocation happens — the heap
    /// pick on the caller side bounds memory regardless of how many
    /// chunks are dirty.
    pub fn drain_updated_chunks(&self) -> impl Iterator<Item = Vec3i> + '_ {
        self.chunks
            .iter()
            .filter(|(_, c)| c.updated())
            .map(|(c, _)| *c)
    }

    /// Clear the `updated` flag on every chunk in `coords`. Coords that
    /// no longer reference loaded chunks are silently skipped. Pair with
    /// [`Self::drain_updated_chunks`]: the caller heap-picks a bounded
    /// subset, rebuilds those meshes, then passes the picked coords here
    /// so they don't reappear in the next iterator pass.
    pub fn clear_updated_chunks(&mut self, coords: &[Vec3i]) {
        for &cc in coords {
            self.with_chunk_mut(cc, crate::chunks::Chunk::clear_updated);
        }
    }

    /// Mark every loaded non-empty chunk's `updated` flag. Used by
    /// `Game::apply_mesh_config` to force a full re-mesh after the
    /// meshing rules (smooth lighting / merge-face / advanced-render)
    /// flip — chunk content hasn't changed, but the geometry needs to
    /// be rebuilt against the new rules.
    pub fn mark_all_loaded_for_remesh(&mut self) {
        let coords: Vec<Vec3i> = self
            .chunks
            .iter()
            .filter(|(_, c)| !c.empty())
            .map(|(c, _)| *c)
            .collect();
        for cc in coords {
            self.with_chunk_mut(cc, crate::chunks::Chunk::mark_neighbor_updated);
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

    /// Slide the load center to `center` (a world coord). Updates the
    /// loaded-core centre, shrinks the core's radius by the chebyshev
    /// distance moved (cells within `radius - move` of the new centre
    /// are also within `radius` of the old centre, so the invariant
    /// survives), and re-pivots the height-map cache so future
    /// generation hits a warm window.
    pub fn set_center(&mut self, center: Vec3i) {
        let center_chunk = chunk_coord(center);
        let mv = center_chunk - self.loaded_core.ccenter;
        let mv_cheb = mv.x.abs().max(mv.y.abs()).max(mv.z.abs());
        self.loaded_core.radius = (self.loaded_core.radius - mv_cheb).max(-1);
        self.loaded_core.ccenter = center_chunk;

        let half: i32 = self.render_distance + 2;
        let new_origin = center_chunk - Vec3i::new(half, half, half);
        self.height_map.set_center(new_origin * Chunk::SIZE);
    }

    /// Synchronously load chunks within
    /// `render_distance + LOAD_RADIUS_BUFFER` of the centre and unload ones
    /// outside `render_distance + LOAD_RADIUS_BUFFER`. The buffer
    /// guarantees every meshable chunk has all six axis neighbours present
    /// so the mesher never samples missing-chunk air at the render boundary.
    /// Caps at [`MAX_CHUNK_LOADS`] loads and [`MAX_CHUNK_UNLOADS`] unloads
    /// per call.
    pub fn tick_chunk_loading(&mut self) {
        for cc in self.collect_load_candidates() {
            self.load_chunk(cc);
        }
        for cc in self.collect_unload_candidates() {
            self.unload_chunk(cc);
            self.unloaded_chunks = self.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Async sibling of [`Self::tick_chunk_loading`]: walks the load/unload
    /// windows the same way but only **issues requests** to the pipeline
    /// worker; never blocks. Pair with [`Self::poll_load_results`] to drain
    /// finished loads. Mirrors C++ `update_chunk_lists`.
    pub fn tick_chunk_loading_async(&mut self) {
        for cc in self.collect_load_candidates() {
            if self.pipeline.request_load(cc) {
                self.in_flight.insert(cc);
            }
        }
        for cc in self.collect_unload_candidates() {
            self.unload_chunk_async(cc);
            self.unloaded_chunks = self.unloaded_chunks.wrapping_add(1);
        }
    }

    /// Bounded-heap shell scan for chunks that should be loaded but
    /// aren't. Uses [`LoadedCore`] to skip the inner shells that are
    /// already known fully loaded; only iterates from the first shell
    /// outside the core out to `render_distance + LOAD_RADIUS_BUFFER`.
    ///
    /// At steady state (player not moving, all chunks resident), the
    /// core has expanded to the window edge and this loop iterates zero
    /// shells — the work is O(1). After a one-chunk slide, only the
    /// outermost shell is scanned (~6·(2·rd)² cells). Compare to the
    /// O((2·rd+3)³) cost of a full re-scan every tick.
    ///
    /// Returns the list of coords to load, ordered closest-first
    /// (matches the previous `sort+truncate` semantics).
    fn collect_load_candidates(&mut self) -> Vec<Vec3i> {
        let center = self.loaded_core.ccenter;
        let load_dist = self.render_distance + LOAD_RADIUS_BUFFER;
        let start = self.loaded_core.radius + 1;
        let mut heap: BinaryHeap<ByDist> = BinaryHeap::with_capacity(MAX_CHUNK_LOADS + 1);
        let mut new_radius = self.loaded_core.radius;

        'shells: for r in start..=load_dist {
            // Iterate cells whose chebyshev distance from `center` is
            // exactly `r` — the surface of the cube of side `2r + 1`.
            // Cells with `|dx| < r && |dy| < r` only contribute the two
            // dz = ±r endpoints; the rest of the cube interior is the
            // already-loaded core.
            for dx in -r..=r {
                for dy in -r..=r {
                    let xy_boundary = dx.abs() == r || dy.abs() == r;
                    if xy_boundary {
                        for dz in -r..=r {
                            Self::try_push_load_candidate(
                                &mut heap,
                                center + Vec3i::new(dx, dy, dz),
                                center,
                                &self.chunks,
                                &self.in_flight,
                            );
                        }
                    } else {
                        for &dz in &[-r, r] {
                            Self::try_push_load_candidate(
                                &mut heap,
                                center + Vec3i::new(dx, dy, dz),
                                center,
                                &self.chunks,
                                &self.in_flight,
                            );
                        }
                    }
                }
            }
            // Heap stays empty as long as every shell scanned so far
            // had no missing chunks. The first shell that finds a gap
            // freezes `new_radius`; deeper shells could still drain
            // closer holes (heap-bounded), but the core can't claim
            // them as fully loaded.
            if heap.is_empty() {
                new_radius = r;
            }
            // Once the heap is full, outer shells are unlikely to
            // dislodge entries (their euclidean distances are mostly
            // larger). Mirror the C++ early break.
            if heap.len() == MAX_CHUNK_LOADS {
                break 'shells;
            }
        }

        self.loaded_core.radius = new_radius;
        // Closest-first iteration order, matching the previous
        // sort+truncate behaviour.
        heap.into_sorted_vec()
            .into_iter()
            .map(|e| e.coord)
            .collect()
    }

    /// Push `cc` onto the bounded load heap if it's not already loaded
    /// or in flight. Free helper — keeps the borrow surface narrow
    /// (`&self.chunks`, `&self.in_flight`) so the caller can still
    /// mutably touch other `self` fields like `self.loaded_core`.
    fn try_push_load_candidate(
        heap: &mut BinaryHeap<ByDist>,
        cc: Vec3i,
        center: Vec3i,
        chunks: &HashMap<Vec3i, Chunk>,
        in_flight: &HashSet<Vec3i>,
    ) {
        if chunks.contains_key(&cc) || in_flight.contains(&cc) {
            return;
        }
        let rel = cc - center;
        let dist = rel.x * rel.x + rel.y * rel.y + rel.z * rel.z;
        heap.push(ByDist { dist, coord: cc });
        if heap.len() > MAX_CHUNK_LOADS {
            heap.pop();
        }
    }

    /// Bounded-heap scan for loaded chunks outside the unload boundary.
    /// Mirrors the previous inline loop; factored out so both
    /// `tick_chunk_loading*` variants share it.
    fn collect_unload_candidates(&self) -> Vec<Vec3i> {
        let center = self.loaded_core.ccenter;
        let unload_dist = self.render_distance + LOAD_RADIUS_BUFFER;
        let mut heap: BinaryHeap<Reverse<ByDist>> =
            BinaryHeap::with_capacity(MAX_CHUNK_UNLOADS + 1);
        for &coord in self.chunks.keys() {
            let d = coord - center;
            if d.x.abs() > unload_dist || d.y.abs() > unload_dist || d.z.abs() > unload_dist {
                let dist = d.x * d.x + d.y * d.y + d.z * d.z;
                heap.push(Reverse(ByDist { dist, coord }));
                if heap.len() > MAX_CHUNK_UNLOADS {
                    heap.pop();
                }
            }
        }
        // `into_sorted_vec` on `Reverse<ByDist>` yields descending
        // distance — farthest first.
        heap.into_sorted_vec()
            .into_iter()
            .map(|Reverse(e)| e.coord)
            .collect()
    }

    /// Drain every available [`LoadResult`] from the pipeline worker, install
    /// each into the hash map, and return the list of inserted coords so the
    /// caller can mark them dirty for meshing.
    pub fn poll_load_results(&mut self) -> Vec<Vec3i> {
        let drained = self.pipeline.drain_results();
        let mut inserted = Vec::with_capacity(drained.len());
        for LoadResult { coord, chunk } in drained {
            self.in_flight.remove(&coord);
            // The world may have moved on while the worker was busy. If the
            // coord is no longer needed, drop the result rather than reviving
            // it. We still insert if the coord falls inside the unload
            // boundary — exact-window-only would lose chunks that arrived
            // just past a slide.
            let half = self.render_distance + LOAD_RADIUS_BUFFER;
            let d = coord - self.loaded_core.ccenter;
            let in_window = d.x.abs() <= half && d.y.abs() <= half && d.z.abs() <= half;
            if !in_window || self.chunks.contains_key(&coord) {
                continue;
            }
            self.chunks.insert(coord, chunk);
            self.mark_chunk_neighbour_chunks_updated(coord);
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
        let save_table = self.chunk_save_table.clone();
        let bytes = self.with_chunk_mut(ccoord, |chunk| {
            if chunk.modified() {
                let bytes = chunk.package_to(&save_table);
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
            self.shrink_loaded_core_for_unload(ccoord);
        }
    }

    /// Insert a chunk: try the on-disk store first, otherwise generate
    /// fresh terrain. The chunk lands in `chunks`; whether `init_generate`
    /// / `unpackage_from` allocated the data array is queryable via
    /// `Chunk::empty()` at the call site.
    fn load_chunk(&mut self, ccoord: Vec3i) {
        if self.chunks.contains_key(&ccoord) {
            return;
        }
        let mut chunk = Chunk::new(ccoord);
        // Disk first: if the tile store has bytes for this coord, restore
        // them. Otherwise generate. Either way `post_init` runs at the end
        // to mirror the C++ flow.
        let from_disk = match self.tiles_store.load(ccoord) {
            Ok(Some(bytes)) => match chunk.unpackage_from(&bytes, &self.chunk_load_table) {
                Ok(()) => true,
                Err(_) => false, // bad bytes — fall through to generate
            },
            _ => false,
        };
        if !from_disk {
            chunk.init_generate(&mut self.height_map, &self.generator, &self.base_blocks);
        }
        self.chunks.insert(ccoord, chunk);
        self.mark_chunk_neighbour_chunks_updated(ccoord);
    }

    /// Save a chunk to disk if dirty, then drop it from the hash map.
    /// Mirrors C++ `_unload_chunk`.
    fn unload_chunk(&mut self, ccoord: Vec3i) {
        let save_table = self.chunk_save_table.clone();
        let bytes = self.with_chunk_mut(ccoord, |chunk| {
            if chunk.modified() {
                let bytes = chunk.package_to(&save_table);
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
            self.shrink_loaded_core_for_unload(ccoord);
        }
    }

    /// Clamp the loaded-core radius after a chunk at `ccoord` has been
    /// removed. The removed cell sits at chebyshev distance `d` from
    /// the core centre, so the core can no longer claim radius `d` —
    /// drop to `d - 1`. Cheap (a few `i32::abs` + `min`); a no-op if
    /// the unload happened outside the current core.
    fn shrink_loaded_core_for_unload(&mut self, ccoord: Vec3i) {
        if self.loaded_core.radius < 0 {
            return;
        }
        let d = ccoord - self.loaded_core.ccenter;
        let dist = d.x.abs().max(d.y.abs()).max(d.z.abs());
        self.loaded_core.radius = self.loaded_core.radius.min(dist - 1);
    }

    /// Hand a `&mut Chunk` for `ccoord` to `f`. Returns `f`'s value, or
    /// `None` when no chunk is loaded at `ccoord`. Centralizes the only
    /// `chunks.get_mut` call site so future world-level write hooks have
    /// one place to land.
    fn with_chunk_mut<F, R>(&mut self, ccoord: Vec3i, f: F) -> Option<R>
    where
        F: FnOnce(&mut Chunk) -> R,
    {
        self.chunks.get_mut(&ccoord).map(f)
    }

    /// Save every modified chunk + the player + world metadata to disk.
    /// Uses the world directory stored at construction time, so this is
    /// independent of cwd. An empty chunk can't be modified (since
    /// `Chunk::modified` is set by `block_mut`, which always allocates), so
    /// the empty-filter is just a fast skip.
    pub fn save_to_disk(&mut self) -> Result<(), WorldError> {
        let coords: Vec<Vec3i> = self
            .chunks
            .iter()
            .filter(|(_, c)| !c.empty() && c.modified())
            .map(|(c, _)| *c)
            .collect();
        let save_table = self.chunk_save_table.clone();
        for cc in coords {
            let bytes = self.with_chunk_mut(cc, |chunk| {
                let bytes = chunk.package_to(&save_table);
                chunk.clear_modified();
                bytes
            });
            if let Some(bytes) = bytes {
                self.tiles_store.save(cc, &bytes)?;
            }
        }
        self.tiles_store.flush()?;
        std::fs::create_dir_all(&self.dir)?;
        // Re-write `world.dat` every save — cheap, and guarantees the file
        // exists for the next load. The mapping itself doesn't change
        // mid-session, so this is a no-op in practice.
        self.world_metadata.save_to(&self.dir.join("world.dat"))?;
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

    fn hitboxes(&self, box_: Aabbd) -> Vec<Aabbd> {
        World::hitboxes(self, box_)
    }

    fn in_water(&self, box_: Aabbd) -> bool {
        World::in_water(self, box_)
    }
}

// World tests live in `rs/tests/world.rs` (integration). They use
// `World::new_at` with an absolute scratch path so they don't depend on
// cwd, which means cargo can run them in parallel with other integration
// test binaries without a chdir race.
