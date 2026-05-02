//! Per-chunk terrain generation. Fills a freshly-air chunk's block
//! array with the standard layered terrain (rock, dirt, grass, sand,
//! water, optional bedrock floor at world `y == 0`).
//!
//! Lives in `core::game::worldgen` rather than `core::world::chunk`
//! because the rules consume registry-aware state (`BaseBlocks`) and
//! the seeded [`HeightNoise`] — neither of which the database half
//! of the engine knows about.

use super::noise::{HeightNoise, WATER_LEVEL};
use crate::blocks::{BaseBlocks, BlockData, Light, State};
use crate::core::world::Blocks;
use crate::core::world::chunk::{SIZE, SIZE_USIZE};
use crate::math::{Vec3i, Vec3u};

/// Generate terrain for the chunk at `coord` into `blocks`. Expects
/// `blocks` to be air-filled at the appropriate default light (use
/// [`Blocks::air_filled`]).
#[allow(clippy::needless_range_loop)] // index loops mirror the C++ port.
pub fn init_generate(blocks: &mut Blocks, coord: Vec3i, noise: &HeightNoise, base: &BaseBlocks) {
    let (heights, low, high) = collect_heights(coord, noise);
    let size = SIZE;

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
            for x in 0..SIZE_USIZE {
                for z in 0..SIZE_USIZE {
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

    for x in 0..SIZE_USIZE {
        for z in 0..SIZE_USIZE {
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
            for y in air_lo..SIZE_USIZE {
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
) -> ([[i32; SIZE_USIZE]; SIZE_USIZE], i32, i32) {
    let mut heights = [[0_i32; SIZE_USIZE]; SIZE_USIZE];
    let mut lo = i32::MAX;
    let mut hi = WATER_LEVEL;
    for (x, row) in heights.iter_mut().enumerate() {
        for (z, slot) in row.iter_mut().enumerate() {
            let world_x = coord.x * SIZE + x as i32;
            let world_z = coord.z * SIZE + z as i32;
            let h = noise.height(world_x, world_z);
            lo = lo.min(h);
            hi = hi.max(h);
            *slot = h;
        }
    }
    let low = (lo - SIZE - 6) / SIZE;
    let high = (hi + SIZE) / SIZE;
    (heights, low, high)
}
