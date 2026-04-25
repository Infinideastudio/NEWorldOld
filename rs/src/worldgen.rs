//! Terrain generation — `Generator` + `HeightMap`.
//!
//! Ports `src/terrain_generation.ixx` (global noise + `get_height`) and
//! `src/height_maps.ixx` (sliding-window 2D height cache) per
//! `docs/rust_migration.md` §4.4. The C++ versions kept their state in
//! module-level globals; here both pieces are owned values constructed by
//! `World`.

use crate::math::Vec3i;

/// Sea level. Mirrors `terrain_generation::WATER_LEVEL` in the C++ build.
pub const WATER_LEVEL: i32 = 96;

const NOISE_SCALE_X: f64 = 64.0;
const NOISE_SCALE_Z: f64 = 64.0;

/// Sentinel meaning "no height cached at this slot". The C++ build uses
/// `-1`; we use `i32::MIN` because real terrain heights can in principle be
/// negative.
const UNCACHED: i32 = i32::MIN;

// ---------- noise primitives ----------

// TODO: the C++ `noise_2d` is a pure deterministic function of `(x, y)` and
// does not consult `_seed` or `_perm`. Until the noise function is reworked
// to actually mix the seed in, `Generator::seed` has no effect on output.
fn noise_2d(x: i32, y: i32) -> f64 {
    // u64 wrapping arithmetic to match the C++ `int64`/`uint64` overflow.
    let mut xx = (x as i64 as u64).wrapping_add((y as i64 as u64).wrapping_mul(13_258_953_287));
    xx = (xx >> 13) ^ xx;
    let v = xx
        .wrapping_mul(xx.wrapping_mul(xx).wrapping_mul(15_731).wrapping_add(789_221))
        .wrapping_add(1_376_312_589)
        & 0x7fff_ffff;
    v as f64 / 16_777_216.0
}

fn interpolated_noise_2d(x: f64, y: f64) -> f64 {
    let int_x = x.floor() as i32;
    let fract_x = x - f64::from(int_x);
    let int_y = y.floor() as i32;
    let fract_y = y - f64::from(int_y);
    let v0 = noise_2d(int_x, int_y);
    let v1 = noise_2d(int_x + 1, int_y);
    let v2 = noise_2d(int_x, int_y + 1);
    let v3 = noise_2d(int_x + 1, int_y + 1);
    lerp(lerp(v0, v1, fract_x), lerp(v2, v3, fract_x), fract_y)
}

fn fractal_noise_2d(x: f64, y: f64) -> f64 {
    let mut total = 0.0;
    let mut frequency = 1.0;
    let mut amplitude = 1.0;
    for _ in 0..=4 {
        total += interpolated_noise_2d(x * frequency, y * frequency) * amplitude;
        frequency *= 2.0;
        amplitude /= 2.0;
    }
    total
}

#[inline]
fn lerp(a: f64, b: f64, t: f64) -> f64 {
    a + (b - a) * t
}

// ---------- Generator ----------

/// Per-world terrain generator. Owns the RNG seed (and, eventually, a
/// permutation table once `noise_2d` is reworked to consume one — see the
/// TODO above `noise_2d`).
#[derive(Clone, Debug)]
pub struct Generator {
    pub seed: u32,
}

impl Generator {
    /// Build a generator for the given world seed.
    #[must_use]
    pub fn new(seed: u32) -> Self {
        Self { seed }
    }

    /// Terrain height at `(x, z)`. Direct port of
    /// `terrain_generation::get_height`.
    #[must_use]
    pub fn height(&self, x: i32, z: i32) -> i32 {
        let xs = f64::from(x) / NOISE_SCALE_X;
        let zs = f64::from(z) / NOISE_SCALE_Z;
        let mountain = fractal_noise_2d(xs / 2.0 + 34.0, zs / 2.0 + 4.0) as i32;
        let upper = fractal_noise_2d(xs + 0.125, zs + 0.125) as i32 / 8 + 96;
        let transition = fractal_noise_2d(xs + 34.0, zs + 4.0) as i32;
        let lower = fractal_noise_2d(xs + 0.125, zs + 0.125) as i32 / 8;
        let base = fractal_noise_2d(xs / 16.0, zs / 16.0) as i32 * 2 - 320;
        if transition > upper {
            if mountain > upper {
                return mountain + base;
            }
            return upper + base;
        }
        if transition < lower {
            return lower + base;
        }
        transition + base
    }
}

// ---------- HeightMap ----------

/// Sliding-window 2D height cache, size N×N (in chunk-pixels, not chunks).
/// `origin` is the lower-corner world coord of the window; `data[x*size + z]`
/// holds the cached height at `(origin.x + x, origin.z + z)`, or
/// [`UNCACHED`] if not yet populated.
#[derive(Clone, Debug)]
pub struct HeightMap {
    size: usize,
    origin: Vec3i,
    data: Vec<i32>,
}

impl HeightMap {
    /// Build a fresh `size × size` window centered on the origin, with every
    /// cell set to the uncached sentinel.
    #[must_use]
    pub fn new(size: usize) -> Self {
        Self {
            size,
            origin: Vec3i::new(0, 0, 0),
            data: vec![UNCACHED; size * size],
        }
    }

    /// Window edge length (in cells).
    #[must_use]
    pub fn size(&self) -> usize {
        self.size
    }

    /// Lower-corner world coordinate of the window. Only `x` and `z` are
    /// meaningful.
    #[must_use]
    pub fn origin(&self) -> Vec3i {
        self.origin
    }

    /// True iff `local.x` and `local.z` are inside `0..size`. The argument
    /// is in *window-local* coordinates (i.e. `world - origin`).
    #[must_use]
    pub fn contains(&self, local: Vec3i) -> bool {
        let n = self.size as i32;
        local.x >= 0 && local.x < n && local.z >= 0 && local.z < n
    }

    /// Shift the window so its new lower corner is `origin`. No-op if
    /// already centered there.
    pub fn set_center(&mut self, origin: Vec3i) {
        if origin != self.origin {
            self.move_to(origin - self.origin);
        }
    }

    /// Look up the cached height at `coord`. If the coord lies inside the
    /// window and is uncached, compute via `generator` and store; if it
    /// lies outside, compute directly without caching.
    pub fn get(&mut self, coord: Vec3i, generator: &Generator) -> i32 {
        let local = coord - self.origin;
        if self.contains(local) {
            let idx = local.x as usize * self.size + local.z as usize;
            if self.data[idx] == UNCACHED {
                self.data[idx] = generator.height(coord.x, coord.z);
            }
            self.data[idx]
        } else {
            generator.height(coord.x, coord.z)
        }
    }

    /// Shift the data array by `offset` and update `origin`. Cells that
    /// fall out of the new window are dropped; cells newly inside the
    /// window are reset to the uncached sentinel.
    fn move_to(&mut self, offset: Vec3i) {
        let n = self.size;
        let mut next = vec![UNCACHED; n * n];
        let ni = n as i32;
        for x in 0..ni {
            for z in 0..ni {
                let src = Vec3i::new(x + offset.x, 0, z + offset.z);
                if src.x >= 0 && src.x < ni && src.z >= 0 && src.z < ni {
                    next[x as usize * n + z as usize] =
                        self.data[src.x as usize * n + src.z as usize];
                }
            }
        }
        self.data = next;
        self.origin += offset;
    }
}

// ---------- Tests ----------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn height_is_deterministic() {
        let g = Generator::new(0xdead_beef);
        let h1 = g.height(17, -42);
        let h2 = g.height(17, -42);
        assert_eq!(h1, h2);
        // A handful of nearby coords come back stable across calls.
        for x in -3..=3 {
            for z in -3..=3 {
                assert_eq!(g.height(x, z), g.height(x, z));
            }
        }
    }

    #[test]
    fn height_produces_sane_values_near_origin() {
        // Sanity: the function returns *some* finite-looking number, and
        // varies (not all the same) across nearby coords.
        let g = Generator::new(0);
        let mut seen = std::collections::HashSet::new();
        for x in -8..=8 {
            for z in -8..=8 {
                let h = g.height(x, z);
                assert!((-10_000..=10_000).contains(&h), "absurd height: {h}");
                seen.insert(h);
            }
        }
        assert!(seen.len() > 1, "height function appears constant");
    }

    #[test]
    fn height_map_caches_lookups() {
        let g = Generator::new(0);
        let mut hm = HeightMap::new(8);
        let coord = Vec3i::new(2, 0, 3);
        // Slot starts as the sentinel.
        let idx = 2 * 8 + 3;
        assert_eq!(hm.data[idx], UNCACHED);
        let h1 = hm.get(coord, &g);
        // After the first get the slot must have been populated.
        assert_ne!(hm.data[idx], UNCACHED);
        assert_eq!(hm.data[idx], h1);
        // A second get returns the same value.
        let h2 = hm.get(coord, &g);
        assert_eq!(h1, h2);
    }

    #[test]
    fn set_center_keeps_overlap_and_drops_outside() {
        let g = Generator::new(0);
        let mut hm = HeightMap::new(4);
        // Populate every cell at the initial origin (0,_,0).
        let mut populated = [[0i32; 4]; 4];
        for (x, row) in populated.iter_mut().enumerate() {
            for (z, cell) in row.iter_mut().enumerate() {
                *cell = hm.get(Vec3i::new(x as i32, 0, z as i32), &g);
            }
        }
        // Slide the window by (+2, 0, +1). New origin = (2,_,1); the
        // overlapping rectangle in window-local space is x in 0..2 and
        // z in 0..3, which in old-window-local space was x in 2..4 and
        // z in 1..4.
        hm.set_center(Vec3i::new(2, 0, 1));
        assert_eq!(hm.origin(), Vec3i::new(2, 0, 1));
        // Overlapping cells preserved.
        for x in 0..2 {
            for z in 0..3 {
                let new_idx = x * 4 + z;
                let old_x = x + 2;
                let old_z = z + 1;
                assert_eq!(
                    hm.data[new_idx], populated[old_x][old_z],
                    "overlap cell ({x},{z}) should preserve old value"
                );
            }
        }
        // Cells outside the overlap reset to the sentinel.
        for x in 2..4 {
            for z in 0..4 {
                assert_eq!(hm.data[x * 4 + z], UNCACHED);
            }
        }
        for x in 0..2 {
            for z in 3..4 {
                assert_eq!(hm.data[x * 4 + z], UNCACHED);
            }
        }
    }

    #[test]
    fn get_outside_window_does_not_cache_or_panic() {
        let g = Generator::new(0);
        let mut hm = HeightMap::new(4);
        // (10, _, 10) is well outside the 0..4 window.
        let coord = Vec3i::new(10, 0, 10);
        let h = hm.get(coord, &g);
        // Direct generator call returns the same value.
        assert_eq!(h, g.height(10, 10));
        // No slot was populated.
        for cell in &hm.data {
            assert_eq!(*cell, UNCACHED);
        }
    }
}
