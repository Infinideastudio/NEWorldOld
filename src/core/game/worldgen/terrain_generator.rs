//! `TerrainGenerator` — holds the height-noise + resolved base-block
//! ids and exposes a [`Self::build_blocks`] entry point that produces
//! fresh chunk content for one coord. Also hosts the per-chunk
//! terrain rules ([`init_generate`]) it calls internally.
//!
//! Lives outside `World` because the database doesn't know about
//! `BaseBlocks`, the block registry, or terrain rules. Callers feed
//! a closure into [`crate::core::world::World::install_chunk`]
//! that, on disk miss, calls back into `build_blocks`.

use std::path::Path;
use std::sync::Arc;
use std::thread::{self, JoinHandle};

use crossbeam_channel::{Receiver, Sender};
use dashmap::DashMap;

use crate::core::blocks::{BlockData, BlockId, BlockLight, BlockRegistry, BlockState};
use crate::core::game::base_blocks::BaseBlocks;
use crate::core::math::{Vec3i, Vec3u};
use crate::core::world::{Chunk, ChunkData, Metadata, WorldError, WorldTables};

use super::erosion::erode;
use super::perlin::noise_map_region;

pub const WATER_LEVEL: i32 = 96;
const TILE_CELLS: i32 = 512;
const TILE_CHUNKS: i32 = 32;
const MARGIN_CELLS: i64 = 64;
const COMPUTE_CELLS: usize = 640;
const EROSION_STEPS: usize = 20;
/// Maps the normalized WorldGen height range [-1, 1] to world y [-48, 240].
const WORLDGEN_HEIGHT_SCALE: f32 = 144.0;
const WORLDGEN_HEIGHT_OFFSET: f32 = 96.0;

pub type TileKey = (i32, i32);

#[derive(Clone)]
pub(crate) struct TileData {
    heights: Vec<f32>,
}

enum WorkerMessage {
    Request(TileKey),
    Stop,
}

// ----------------------------------------------------------------------
//   TerrainGenerator
// ----------------------------------------------------------------------

pub struct TerrainGenerator {
    base: BaseBlocks,
    #[allow(dead_code)]
    registry: Arc<BlockRegistry>,
    tiles: Arc<DashMap<TileKey, TileData>>,
    request_tx: Sender<WorkerMessage>,
    result_rx: Receiver<(TileKey, TileData)>,
    worker: Option<JoinHandle<()>>,
}

/// Build the [`WorldTables`] for the world named `name` rooted at
/// `root` against the in-memory `registry`. Loads `world.dat` if
/// present (translating canonical ↔ current ids); otherwise
/// snapshots from `registry`. Identity tables collapse to empty
/// `Vec`s so the chunk codec can take the fast path.
pub fn world_tables_for(
    root: &Path,
    name: &str,
    registry: &BlockRegistry,
) -> Result<WorldTables, WorldError> {
    world_tables_for_seed(root, name, registry, 0)
}

pub fn world_tables_for_seed(
    root: &Path,
    name: &str,
    registry: &BlockRegistry,
    seed: u32,
) -> Result<WorldTables, WorldError> {
    let dir = root.join("worlds").join(name);
    let metadata_path = dir.join("world.dat");
    if metadata_path.exists() {
        let mut metadata = Metadata::load_from(&metadata_path)?;
        if metadata.seed_needs_migration {
            metadata.seed = seed;
            metadata.seed_needs_migration = false;
        }
        let load = metadata.canonical_to_current(registry);
        let save = metadata.current_to_canonical(registry);
        let load_identity = load.len() == registry.len()
            && load
                .iter()
                .enumerate()
                .all(|(i, id)| id.get() as usize == i);
        let save_identity = save.len() == registry.len()
            && save.iter().enumerate().all(|(i, &c)| c.get() as usize == i);
        Ok(WorldTables {
            metadata,
            load_table: if load_identity { Vec::new() } else { load },
            save_table: if save_identity { Vec::new() } else { save },
        })
    } else {
        Ok(WorldTables {
            metadata: Metadata::from_registry(registry, seed),
            load_table: Vec::new(),
            save_table: Vec::new(),
        })
    }
}

impl TerrainGenerator {
    pub fn new(registry: Arc<BlockRegistry>, base_blocks: BaseBlocks, seed: u32) -> Self {
        let (request_tx, request_rx) = crossbeam_channel::unbounded();
        let (result_tx, result_rx) = crossbeam_channel::unbounded();
        let worker = thread::Builder::new()
            .name("neworld-worldgen-worker".to_string())
            .spawn(move || {
                while let Ok(message) = request_rx.recv() {
                    let WorkerMessage::Request((tx, tz)) = message else {
                        break;
                    };
                    let origin_x = i64::from(tx) * i64::from(TILE_CELLS) - MARGIN_CELLS;
                    let origin_z = i64::from(tz) * i64::from(TILE_CELLS) - MARGIN_CELLS;
                    let raw = noise_map_region(
                        origin_x,
                        origin_z,
                        COMPUTE_CELLS,
                        COMPUTE_CELLS,
                        seed,
                        1.0,
                    );
                    let eroded = erode(&raw, EROSION_STEPS);
                    let mut heights = Vec::with_capacity((TILE_CELLS * TILE_CELLS) as usize);
                    for row in eroded
                        .iter()
                        .skip(MARGIN_CELLS as usize)
                        .take(TILE_CELLS as usize)
                    {
                        heights.extend_from_slice(
                            &row[MARGIN_CELLS as usize
                                ..MARGIN_CELLS as usize + TILE_CELLS as usize],
                        );
                    }
                    if result_tx.send(((tx, tz), TileData { heights })).is_err() {
                        break;
                    }
                }
            })
            .expect("worldgen worker spawn");
        Self {
            base: base_blocks,
            registry,
            tiles: Arc::new(DashMap::new()),
            request_tx,
            result_rx,
            worker: Some(worker),
        }
    }

    pub fn tile_for_chunk(coord: Vec3i) -> TileKey {
        (
            coord.x.div_euclid(TILE_CHUNKS),
            coord.z.div_euclid(TILE_CHUNKS),
        )
    }

    pub fn request_tile(&self, tile: TileKey) {
        let _ = self.request_tx.send(WorkerMessage::Request(tile));
    }

    pub fn drain_results(&mut self) -> Vec<TileKey> {
        let mut ready = Vec::new();
        while let Ok((tile, data)) = self.result_rx.try_recv() {
            self.tiles.insert(tile, data);
            ready.push(tile);
        }
        ready
    }

    pub fn has_tile(&self, tile: TileKey) -> bool {
        self.tiles.contains_key(&tile)
    }

    /// Generate fresh chunk content for `coord`. Intended to be
    /// invoked from inside the closure passed to
    /// [`crate::core::world::World::install_chunk`] — World only
    /// calls the closure on a disk miss, so this never runs for
    /// chunks already on disk.
    pub fn build_blocks(&self, coord: Vec3i) -> ChunkData {
        let mut blocks = ChunkData::default();
        init_generate(&mut blocks, coord, &self.tiles, &self.base);
        blocks
    }
}

impl Drop for TerrainGenerator {
    fn drop(&mut self) {
        let _ = self.request_tx.send(WorkerMessage::Stop);
        if let Some(worker) = self.worker.take() {
            let _ = worker.join();
        }
    }
}

// ----------------------------------------------------------------------
//   Per-chunk terrain rules
// ----------------------------------------------------------------------

/// Generate terrain for the chunk at `coord` into `blocks`. Expects
/// `blocks` to be air-filled at the appropriate default light (use
/// [`Blocks::air_filled`]). Fills the standard layered terrain (rock,
/// dirt, grass, sand, water, optional bedrock floor at world `y == 0`).
#[allow(clippy::needless_range_loop)] // index loops mirror the C++ port.
pub(super) fn init_generate(
    blocks: &mut ChunkData,
    coord: Vec3i,
    tiles: &DashMap<TileKey, TileData>,
    base: &BaseBlocks,
) {
    let (heights, low, high) = collect_heights(coord, tiles);
    let size = Chunk::SIZE as i32;

    // World y < 0 is below the bedrock layer and is always solid rock.
    if coord.y < 0 {
        let rock = BlockData {
            id: base.rock,
            state: BlockState::default(),
            light: BlockLight::default(),
        };
        for cell in blocks.iter_mut() {
            *cell = rock;
        }
        return;
    }

    // Skip generation: chunk is fully above terrain & water surface.
    if coord.y > high && coord.y * size > WATER_LEVEL {
        return;
    }

    // Fully below the lowest terrain column: solid rock.
    if coord.y < low {
        let rock = BlockData {
            id: base.rock,
            state: BlockState::default(),
            light: BlockLight::default(),
        };
        for cell in blocks.iter_mut() {
            *cell = rock;
        }
        return;
    }

    // Normal generation: mixed terrain + water + air column-by-column.
    let sh = WATER_LEVEL + 2 - (coord.y * size);
    let wh = WATER_LEVEL - (coord.y * size);

    for x in 0..Chunk::SIZE {
        for z in 0..Chunk::SIZE {
            let h = heights[x][z] - (coord.y * size);

            if h > sh && h > wh + 1 {
                if h >= 0 && h < size {
                    blocks
                        .block_mut(Vec3u::new(x as u32, h as u32, z as u32))
                        .id = base.grass;
                }
                let dirt_lo = (h - 5).max(0).min(size) as usize;
                let dirt_hi = h.max(0).min(size) as usize;
                for y in dirt_lo..dirt_hi {
                    blocks
                        .block_mut(Vec3u::new(x as u32, y as u32, z as u32))
                        .id = base.dirt;
                }
            } else {
                let sand_lo = (h - 5).max(0).min(size) as usize;
                let sand_hi = (h + 1).max(0).min(size) as usize;
                for y in sand_lo..sand_hi {
                    blocks
                        .block_mut(Vec3u::new(x as u32, y as u32, z as u32))
                        .id = base.sand;
                }
                let minh = (h + 1).max(0).min(size);
                let maxh = (wh + 1).max(0).min(size);
                let mut sky = (15 - (WATER_LEVEL - (maxh - 1 + (coord.y * size)))).max(0);
                let mut y = maxh - 1;
                while y >= minh {
                    sky = (sky - 1).max(0);
                    let cell = blocks.block_mut(Vec3u::new(x as u32, y as u32, z as u32));
                    cell.id = base.water;
                    cell.light = BlockLight::sky_and_block(sky as u8, 0);
                    y -= 1;
                }
            }

            let rock_hi = (h - 5).max(0).min(size) as usize;
            for y in 0..rock_hi {
                blocks
                    .block_mut(Vec3u::new(x as u32, y as u32, z as u32))
                    .id = base.rock;
            }

            let air_lo = (h.max(wh) + 1).max(0).min(size) as usize;
            for y in air_lo..Chunk::SIZE {
                let cell = blocks.block_mut(Vec3u::new(x as u32, y as u32, z as u32));
                cell.id = BlockId::default();
                cell.light = BlockLight::sky_and_block(15, 0);
            }

            if coord.y == 0 {
                blocks.block_mut(Vec3u::new(x as u32, 0, z as u32)).id = base.bedrock;
            }
        }
    }
}

fn collect_heights(
    coord: Vec3i,
    tiles: &DashMap<TileKey, TileData>,
) -> ([[i32; Chunk::SIZE]; Chunk::SIZE], i32, i32) {
    let size = Chunk::SIZE as i32;
    let mut heights = [[0_i32; Chunk::SIZE]; Chunk::SIZE];
    let mut lo = i32::MAX;
    let mut hi = WATER_LEVEL;
    for (x, row) in heights.iter_mut().enumerate() {
        for (z, slot) in row.iter_mut().enumerate() {
            let world_x = coord.x * size + x as i32;
            let world_z = coord.z * size + z as i32;
            let tile = (
                coord.x.div_euclid(TILE_CHUNKS),
                coord.z.div_euclid(TILE_CHUNKS),
            );
            let local_x = world_x.rem_euclid(TILE_CELLS) as usize;
            let local_z = world_z.rem_euclid(TILE_CELLS) as usize;
            let noise_value = tiles
                .get(&tile)
                .map(|data| data.heights[local_z * TILE_CELLS as usize + local_x])
                .unwrap_or(0.0);
            let h = (noise_value * WORLDGEN_HEIGHT_SCALE + WORLDGEN_HEIGHT_OFFSET).round() as i32;
            lo = lo.min(h);
            hi = hi.max(h);
            *slot = h;
        }
    }
    let low = (lo - size - 6) / size;
    let high = (hi + size) / size;
    (heights, low, high)
}

#[cfg(test)]
mod erosion_measurement_tests {
    use super::{COMPUTE_CELLS, EROSION_STEPS, MARGIN_CELLS, TILE_CELLS};
    use crate::core::game::worldgen::erosion::erode;
    use crate::core::game::worldgen::perlin::noise_map_region;

    #[test]
    fn measure_selected_tile_erosion_depth() {
        let seed = 0xDEAD_BEEFu32;
        let tile = (0_i32, 0_i32);
        let origin_x = i64::from(tile.0) * i64::from(TILE_CELLS) - MARGIN_CELLS;
        let origin_z = i64::from(tile.1) * i64::from(TILE_CELLS) - MARGIN_CELLS;
        let raw = noise_map_region(origin_x, origin_z, COMPUTE_CELLS, COMPUTE_CELLS, seed, 1.0);
        let eroded = erode(&raw, EROSION_STEPS);
        let margin = MARGIN_CELLS as usize;
        let width = TILE_CELLS as usize;
        let mut depths = Vec::with_capacity(width * width);
        for z in margin..margin + width {
            for x in margin..margin + width {
                depths.push(raw[z][x] - eroded[z][x]);
            }
        }
        depths.sort_by(f32::total_cmp);
        let max = *depths.last().expect("non-empty tile");
        let top_one_percent_start = depths.len() * 99 / 100;
        let top_one_percent = &depths[top_one_percent_start..];
        let top_one_percent_mean =
            top_one_percent.iter().sum::<f32>() / top_one_percent.len() as f32;
        let percentile_99 = depths[top_one_percent_start];
        const WORLD_Y_PER_NOISE_UNIT: f32 = 96.0;
        println!(
            "tile={tile:?} seed={seed:#010x} cells={} max_depth_noise={max:.9} max_depth_world_y={:.6} top_1_percent_mean_noise={top_one_percent_mean:.9} top_1_percent_mean_world_y={:.6} p99_noise={percentile_99:.9} p99_world_y={:.6}",
            depths.len(),
            max * WORLD_Y_PER_NOISE_UNIT,
            top_one_percent_mean * WORLD_Y_PER_NOISE_UNIT,
            percentile_99 * WORLD_Y_PER_NOISE_UNIT,
        );
    }
}
