//! WorldGen's lattice and gradient Perlin implementation.

const GRADIENTS: [[f32; 2]; 32] = [
    [0.9951847, 0.0980171],
    [0.9569403, 0.2902847],
    [0.8819213, 0.4713967],
    [0.7730105, 0.6343933],
    [0.6343933, 0.7730105],
    [0.4713967, 0.8819213],
    [0.2902847, 0.9569403],
    [0.0980171, 0.9951847],
    [-0.0980171, 0.9951847],
    [-0.2902847, 0.9569403],
    [-0.4713967, 0.8819213],
    [-0.6343933, 0.7730105],
    [-0.7730105, 0.6343933],
    [-0.8819213, 0.4713967],
    [-0.9569403, 0.2902847],
    [-0.9951847, 0.0980171],
    [-0.9951847, -0.0980171],
    [-0.9569403, -0.2902847],
    [-0.8819213, -0.4713967],
    [-0.7730105, -0.6343933],
    [-0.6343933, -0.7730105],
    [-0.4713967, -0.8819213],
    [-0.2902847, -0.9569403],
    [-0.0980171, -0.9951847],
    [0.0980171, -0.9951847],
    [0.2902847, -0.9569403],
    [0.4713967, -0.8819213],
    [0.6343933, -0.7730105],
    [0.7730105, -0.6343933],
    [0.8819213, -0.4713967],
    [0.9569403, -0.2902847],
    [0.9951847, -0.0980171],
];

#[derive(Clone)]
pub struct LatticeNoise {
    seed: u32,
}

impl LatticeNoise {
    pub fn new(seed: u32) -> Self {
        Self { seed: hash(seed) }
    }

    pub fn noise(&self, x: i32, y: i32) -> u8 {
        let mut v = (x as u32)
            .wrapping_mul(7177)
            .wrapping_add((y as u32).wrapping_mul(1723957));
        v = (v >> 13) ^ v ^ self.seed;
        v = v
            .wrapping_mul(v.wrapping_mul(v).wrapping_mul(15731).wrapping_add(789221))
            .wrapping_add(1376312589);
        ((v & 0x7fffffff) / 67108864) as u8
    }
}

fn hash(mut x: u32) -> u32 {
    x = (!x).wrapping_add(x.wrapping_shl(15));
    x ^= x >> 12;
    x = x.wrapping_add(x.wrapping_shl(2));
    x ^= x >> 4;
    x = x.wrapping_mul(2057);
    x ^ (x >> 16)
}

#[derive(Clone)]
pub struct PerlinNoise {
    lattice: LatticeNoise,
}

impl PerlinNoise {
    pub fn new(seed: u32) -> Self {
        Self {
            lattice: LatticeNoise::new(seed),
        }
    }

    fn lerp(t: f32, a: f32, b: f32) -> f32 {
        let t = ((6.0 * t - 15.0) * t + 10.0) * t * t * t;
        a + (b - a) * t
    }

    fn dot(index: u8, dx: f32, dy: f32) -> f32 {
        let g = GRADIENTS[index as usize];
        g[0] * dx + g[1] * dy
    }

    pub fn noise(&self, x: f32, y: f32) -> f32 {
        let ix = x.floor() as i32;
        let iy = y.floor() as i32;
        let u = x - ix as f32;
        let v = y - iy as f32;
        let a = Self::lerp(
            u,
            Self::dot(self.lattice.noise(ix, iy), u, v),
            Self::dot(self.lattice.noise(ix + 1, iy), u - 1.0, v),
        );
        let b = Self::lerp(
            u,
            Self::dot(self.lattice.noise(ix, iy + 1), u, v - 1.0),
            Self::dot(self.lattice.noise(ix + 1, iy + 1), u - 1.0, v - 1.0),
        );
        Self::lerp(v, a, b)
    }
}

const NUM_OCTAVES: usize = 6;
/// World-space horizontal scale of the base octave: one noise unit spans
/// 512 world blocks.
pub const BLOCKS_PER_NOISE_UNIT: f32 = 512.0;
const CELL_NX: f32 = 1.0 / BLOCKS_PER_NOISE_UNIT;

fn octave_seed(seed: u32, octave: usize) -> u32 {
    let mut x = seed.wrapping_add((octave as u32).wrapping_mul(0x9e3779b9));
    x ^= x >> 16;
    x = x.wrapping_mul(0x85ebca6b);
    x ^= x >> 13;
    x.wrapping_mul(0xc2b2ae35) ^ (x >> 16)
}

pub fn noise_map_region(
    origin_x: i64,
    origin_z: i64,
    width: usize,
    height: usize,
    seed: u32,
    zoom: f32,
) -> Vec<Vec<f32>> {
    let mut map = vec![vec![0.0; width]; height];
    let mut amplitude = 1.0;
    for octave in 0..NUM_OCTAVES {
        let perlin = PerlinNoise::new(octave_seed(seed, octave));
        for (z, row) in map.iter_mut().enumerate() {
            let nz = (origin_z + z as i64) as f32 * CELL_NX * amplitude / zoom;
            for (x, value) in row.iter_mut().enumerate() {
                let nx = (origin_x + x as i64) as f32 * CELL_NX * amplitude / zoom;
                *value += perlin.noise(nx, nz) / amplitude;
            }
        }
        amplitude *= 2.0;
    }
    map
}

#[cfg(test)]
mod tests {
    use super::noise_map_region;

    #[test]
    fn adjacent_regions_share_boundary_values() {
        let left = noise_map_region(0, 0, 16, 16, 7, 1.0);
        let right = noise_map_region(16, 0, 16, 16, 7, 1.0);
        let full = noise_map_region(0, 0, 32, 16, 7, 1.0);
        for z in 0..16 {
            assert!((left[z][15] - full[z][15]).abs() < 1e-6);
            assert!((right[z][0] - full[z][16]).abs() < 1e-6);
        }
    }
}
