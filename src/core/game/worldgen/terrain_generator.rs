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

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, Light, State};
use crate::core::world::{Chunk, ChunkData, Metadata, WorldError, WorldTables};
use crate::math::{Vec3i, Vec3u};

use super::noise::{HeightNoise, WATER_LEVEL};

// ----------------------------------------------------------------------
//   TerrainGenerator
// ----------------------------------------------------------------------

pub struct TerrainGenerator {
    noise: HeightNoise,
    base: BaseBlocks,
    #[allow(dead_code)]
    registry: Arc<BlockRegistry>,
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
    let dir = root.join("worlds").join(name);
    let metadata_path = dir.join("world.dat");
    if metadata_path.exists() {
        let metadata = Metadata::load_from(&metadata_path)?;
        let load = metadata.canonical_to_current(registry);
        let save = metadata.current_to_canonical(registry);
        let load_identity = load.len() == registry.len()
            && load.iter().enumerate().all(|(i, id)| id.0 as usize == i);
        let save_identity =
            save.len() == registry.len() && save.iter().enumerate().all(|(i, &c)| c as usize == i);
        Ok(WorldTables {
            metadata,
            load_table: if load_identity { Vec::new() } else { load },
            save_table: if save_identity { Vec::new() } else { save },
        })
    } else {
        Ok(WorldTables {
            metadata: Metadata::from_registry(registry),
            load_table: Vec::new(),
            save_table: Vec::new(),
        })
    }
}

impl TerrainGenerator {
    pub fn new(registry: Arc<BlockRegistry>, base_blocks: BaseBlocks, seed: u32) -> Self {
        Self {
            noise: HeightNoise::new(seed),
            base: base_blocks,
            registry,
        }
    }

    /// Generate fresh chunk content for `coord`. Intended to be
    /// invoked from inside the closure passed to
    /// [`crate::core::world::World::install_chunk`] — World only
    /// calls the closure on a disk miss, so this never runs for
    /// chunks already on disk.
    pub fn build_blocks(&self, coord: Vec3i) -> ChunkData {
        let default_light = if coord.y < 0 { Light::NONE } else { Light::SKY };
        let mut blocks = ChunkData::air_filled(default_light);
        init_generate(&mut blocks, coord, &self.noise, &self.base);
        blocks
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
    noise: &HeightNoise,
    base: &BaseBlocks,
) {
    let (heights, low, high) = collect_heights(coord, noise);
    let size = Chunk::SIZE as i32;

    // Skip generation: chunk is fully above terrain & water surface.
    if coord.y < 0 || (coord.y > high && coord.y * size > WATER_LEVEL) {
        return;
    }

    // Fully below the lowest terrain column: solid rock.
    if coord.y < low {
        let rock = BlockData {
            id: base.rock,
            state: State(0),
            light: Light::NONE,
        };
        for cell in blocks.iter_mut() {
            *cell = rock;
        }
        if coord.y == 0 {
            for x in 0..Chunk::SIZE {
                for z in 0..Chunk::SIZE {
                    blocks.block_mut(Vec3u::new(x as u32, 0, z as u32)).id = base.bedrock;
                }
            }
        }
        return;
    }

    // Normal generation: mixed terrain + water + air column-by-column.
    let air_no_light = BlockData {
        id: base.air,
        state: State(0),
        light: Light::NONE,
    };
    for cell in blocks.iter_mut() {
        *cell = air_no_light;
    }

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
                let mut sky = (i32::from(Light::SKY.sky())
                    - (WATER_LEVEL - (maxh - 1 + (coord.y * size))))
                    .max(0);
                let mut y = maxh - 1;
                while y >= minh {
                    sky = (sky - 1).max(0);
                    let cell = blocks.block_mut(Vec3u::new(x as u32, y as u32, z as u32));
                    cell.id = base.water;
                    cell.light = Light::new(sky as u8, Light::SKY.block());
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
                cell.id = base.air;
                cell.light = Light::SKY;
            }

            if coord.y == 0 {
                blocks.block_mut(Vec3u::new(x as u32, 0, z as u32)).id = base.bedrock;
            }
        }
    }
}

fn collect_heights(
    coord: Vec3i,
    noise: &HeightNoise,
) -> ([[i32; Chunk::SIZE]; Chunk::SIZE], i32, i32) {
    let size = Chunk::SIZE as i32;
    let mut heights = [[0_i32; Chunk::SIZE]; Chunk::SIZE];
    let mut lo = i32::MAX;
    let mut hi = WATER_LEVEL;
    for (x, row) in heights.iter_mut().enumerate() {
        for (z, slot) in row.iter_mut().enumerate() {
            let world_x = coord.x * size + x as i32;
            let world_z = coord.z * size + z as i32;
            let h = noise.height(world_x, world_z);
            lo = lo.min(h);
            hi = hi.max(h);
            *slot = h;
        }
    }
    let low = (lo - size - 6) / size;
    let high = (hi + size) / size;
    (heights, low, high)
}
