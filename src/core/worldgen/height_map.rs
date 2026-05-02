//! Sliding-window 2D height cache — direct port of
//! `src/height_maps.ixx`.
//!
//! `World` owns one of these and slides it as the player walks (so chunk
//! generation hits a warm cache when neighbouring chunks ask for the same
//! `(x, z)` heights). The C++ original lived in module-level state; here
//! it's a plain owned value.

use crate::math::Vec3i;
use super::generator::Generator;

/// Sentinel meaning "no height cached at this slot". The C++ build uses
/// `-1`; we use `i32::MIN` because real terrain heights can in principle be
/// negative.
const UNCACHED: i32 = i32::MIN;

/// Sliding-window 2D height cache, size N×N (in cells, not chunks).
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn caches_lookups() {
        let g = Generator::new(0);
        let mut hm = HeightMap::new(8);
        let coord = Vec3i::new(2, 0, 3);
        let h1 = hm.get(coord, &g);
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
