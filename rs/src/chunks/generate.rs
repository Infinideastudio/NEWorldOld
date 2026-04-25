//! Terrain generation for chunks.
//!
//! Extends [`super::Chunk`] with an `init_generate` method that fills the
//! chunk with terrain blocks from a [`Generator`] / [`HeightMap`] pair. Ports
//! `chunks.ixx::_generate_terrain` from the C++ build faithfully — same
//! column-height short-circuits, same layering, same bedrock override at
//! `coord.y == 0`.

use super::Chunk;
use crate::blocks::{BaseBlocks, BlockData, Light, State};
use crate::math::Vec3i;
use crate::worldgen::{Generator, HeightMap, WATER_LEVEL};

impl Chunk {
    /// Read the 16×16 column heights for this chunk and compute the derived
    /// `low` / `high` chunk-Y bounds used to short-circuit generation.
    /// Mirrors C++ `_heights`.
    fn collect_heights(
        &self,
        height_map: &mut HeightMap,
        generator: &Generator,
    ) -> (
        [[i32; Self::SIZE_USIZE]; Self::SIZE_USIZE],
        i32, // low
        i32, // high
    ) {
        let mut heights = [[0_i32; Self::SIZE_USIZE]; Self::SIZE_USIZE];
        let mut lo = i32::MAX;
        let mut hi = WATER_LEVEL;
        for (x, row) in heights.iter_mut().enumerate() {
            for (z, slot) in row.iter_mut().enumerate() {
                let world_x = self.coord.x * Self::SIZE + x as i32;
                let world_z = self.coord.z * Self::SIZE + z as i32;
                let h = height_map.get(Vec3i::new(world_x, 0, world_z), generator);
                lo = lo.min(h);
                hi = hi.max(h);
                *slot = h;
            }
        }
        let low = (lo - Self::SIZE - 6) / Self::SIZE;
        let high = (hi + Self::SIZE) / Self::SIZE;
        (heights, low, high)
    }

    /// Generate this chunk's terrain.
    #[allow(clippy::needless_range_loop)] // index loops mirror the C++ port and
    // also avoid aliasing self.cell_mut(...) with iterators over self.data.
    pub fn init_generate(
        &mut self,
        height_map: &mut HeightMap,
        generator: &Generator,
        base: &BaseBlocks,
    ) {
        let (heights, low, high) = self.collect_heights(height_map, generator);
        let size = Self::SIZE;

        // Skip generation: chunk is fully above terrain & water surface.
        if self.coord.y < 0 || (self.coord.y > high && self.coord.y * size > WATER_LEVEL) {
            return;
        }

        // Fully below the lowest terrain column: solid rock with optional
        // bedrock floor at world y=0.
        if self.coord.y < low {
            self.ensure_data();
            let rock = BlockData {
                id: base.rock,
                state: State(0),
                light: Light::NONE,
            };
            for cell in self
                .data
                .as_mut()
                .expect("ensure_data allocated")
                .iter_mut()
            {
                *cell = rock;
            }
            if self.coord.y == 0 {
                for x in 0..Self::SIZE_USIZE {
                    for z in 0..Self::SIZE_USIZE {
                        // Index of (x, 0, z): x*SIZE*SIZE + 0*SIZE + z
                        let idx = x * Self::SIZE_USIZE * Self::SIZE_USIZE + z;
                        self.cell_mut(idx).id = base.bedrock;
                    }
                }
            }
            self.empty = false;
            return;
        }

        // Normal generation: mixed terrain + water + air column-by-column.
        self.ensure_data();
        let air_no_light = BlockData {
            id: base.air,
            state: State(0),
            light: Light::NONE,
        };
        for cell in self
            .data
            .as_mut()
            .expect("ensure_data allocated")
            .iter_mut()
        {
            *cell = air_no_light;
        }

        let sh = WATER_LEVEL + 2 - (self.coord.y * size);
        let wh = WATER_LEVEL - (self.coord.y * size);
        let size_us = Self::SIZE_USIZE;

        for x in 0..size_us {
            for z in 0..size_us {
                let base_idx = x * size_us * size_us + z;
                let h = heights[x][z] - (self.coord.y * size);
                if h >= 0 || wh >= 0 {
                    self.empty = false;
                }

                if h > sh && h > wh + 1 {
                    // Grass top.
                    if h >= 0 && h < size {
                        self.cell_mut(((h as usize) * size_us) + base_idx).id = base.grass;
                    }
                    // Dirt layer below the grass top.
                    let dirt_lo = (h - 5).max(0).min(size) as usize;
                    let dirt_hi = h.max(0).min(size) as usize;
                    for y in dirt_lo..dirt_hi {
                        self.cell_mut((y * size_us) + base_idx).id = base.dirt;
                    }
                } else {
                    // Sand layer (just below water level).
                    let sand_lo = (h - 5).max(0).min(size) as usize;
                    let sand_hi = (h + 1).max(0).min(size) as usize;
                    for y in sand_lo..sand_hi {
                        self.cell_mut((y * size_us) + base_idx).id = base.sand;
                    }
                    // Water column.
                    let minh = (h + 1).max(0).min(size);
                    let maxh = (wh + 1).max(0).min(size);
                    let mut sky = (i32::from(Light::SKY.sky())
                        - (WATER_LEVEL - (maxh - 1 + (self.coord.y * size))))
                        .max(0);
                    let mut y = maxh - 1;
                    while y >= minh {
                        sky = (sky - 1).max(0);
                        let cell = self.cell_mut(((y as usize) * size_us) + base_idx);
                        cell.id = base.water;
                        cell.light = Light::new(sky as u8, Light::SKY.block());
                        y -= 1;
                    }
                }

                // Rock layer at the bottom of every column.
                let rock_hi = (h - 5).max(0).min(size) as usize;
                for y in 0..rock_hi {
                    self.cell_mut((y * size_us) + base_idx).id = base.rock;
                }

                // Air-with-sky-light above the surface (and above water).
                let air_lo = (h.max(wh) + 1).max(0).min(size) as usize;
                for y in air_lo..size_us {
                    let cell = self.cell_mut((y * size_us) + base_idx);
                    cell.id = base.air;
                    cell.light = Light::SKY;
                }

                // Bedrock floor at world y=0, overriding rock.
                if self.coord.y == 0 {
                    self.cell_mut(base_idx).id = base.bedrock;
                }
            }
        }
    }
}
