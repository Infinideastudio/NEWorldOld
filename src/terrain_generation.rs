//! Terrain generation — `Generator` + the per-world noise primitives.
//!
//! Direct port of `src/terrain_generation.ixx` per
//! `docs/rust_migration.md` §4.4. The C++ original kept its state in
//! module-level globals; here `Generator` owns the seed (and, eventually, a
//! permutation table once `noise_2d` is reworked to actually consume it).
//!
//! The sliding-window 2D height cache lives next door in
//! [`crate::height_maps`] — same split as the C++ build, where
//! `terrain_generation.ixx` and `height_maps.ixx` are sibling modules.

/// Sea level. Mirrors `terrain_generation::WATER_LEVEL` in the C++ build.
pub const WATER_LEVEL: i32 = 96;

const NOISE_SCALE_X: f64 = 64.0;
const NOISE_SCALE_Z: f64 = 64.0;

// ---------- noise primitives ----------

// Wang-style seed mix constant (golden-ratio multiple of 2^64). The original
// C++ `noise_2d` only hashed `(x, y)`, so different worlds produced identical
// terrain — this xor-shift-multiply lifts the seed into the 64-bit hash so
// `Generator::seed` actually controls the output.
const SEED_MIX: u64 = 0x9E37_79B9_7F4A_7C15;

fn noise_2d(seed: u32, x: i32, y: i32) -> f64 {
    // u64 wrapping arithmetic to match the C++ `int64`/`uint64` overflow.
    let mut xx = (x as i64 as u64).wrapping_add((y as i64 as u64).wrapping_mul(13_258_953_287));
    xx ^= u64::from(seed).wrapping_mul(SEED_MIX);
    xx = (xx >> 13) ^ xx;
    let v = xx
        .wrapping_mul(xx.wrapping_mul(xx).wrapping_mul(15_731).wrapping_add(789_221))
        .wrapping_add(1_376_312_589)
        & 0x7fff_ffff;
    v as f64 / 16_777_216.0
}

fn interpolated_noise_2d(seed: u32, x: f64, y: f64) -> f64 {
    let int_x = x.floor() as i32;
    let fract_x = x - f64::from(int_x);
    let int_y = y.floor() as i32;
    let fract_y = y - f64::from(int_y);
    let v0 = noise_2d(seed, int_x, int_y);
    let v1 = noise_2d(seed, int_x + 1, int_y);
    let v2 = noise_2d(seed, int_x, int_y + 1);
    let v3 = noise_2d(seed, int_x + 1, int_y + 1);
    lerp(lerp(v0, v1, fract_x), lerp(v2, v3, fract_x), fract_y)
}

fn fractal_noise_2d(seed: u32, x: f64, y: f64) -> f64 {
    let mut total = 0.0;
    let mut frequency = 1.0;
    let mut amplitude = 1.0;
    for _ in 0..=4 {
        total += interpolated_noise_2d(seed, x * frequency, y * frequency) * amplitude;
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

/// Per-world terrain generator. Owns the RNG seed; `noise_2d` mixes it
/// into the hash via [`SEED_MIX`].
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
        let s = self.seed;
        let mountain = fractal_noise_2d(s, xs / 2.0 + 34.0, zs / 2.0 + 4.0) as i32;
        let upper = fractal_noise_2d(s, xs + 0.125, zs + 0.125) as i32 / 8 + 96;
        let transition = fractal_noise_2d(s, xs + 34.0, zs + 4.0) as i32;
        let lower = fractal_noise_2d(s, xs + 0.125, zs + 0.125) as i32 / 8;
        let base = fractal_noise_2d(s, xs / 16.0, zs / 16.0) as i32 * 2 - 320;
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
    fn different_seeds_produce_different_terrain() {
        let g1 = Generator::new(1);
        let g2 = Generator::new(2);
        let mut diffs = 0;
        for x in -8..=8 {
            for z in -8..=8 {
                if g1.height(x, z) != g2.height(x, z) {
                    diffs += 1;
                }
            }
        }
        assert!(
            diffs > 100,
            "expected most heights to differ across seeds, got {diffs}/289"
        );
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
}
