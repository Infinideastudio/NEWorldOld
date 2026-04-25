//! Sliding-window `ChunkGrid` — the C++ `ChunkPointerArray` reborn.
//!
//! See `docs/rust_migration.md` §2.2 / §2.4. Cells store [`ChunkKey`]s (slab
//! indices in [`super::World::chunks`]); cells outside the window return
//! `None`, and out-of-range writes are no-ops.

use crate::math::Vec3i;

/// Slab index, scoped to one [`super::World`]. **Never persisted, never sent
/// across threads as a deferred reference** — see `docs/rust_migration.md`
/// §2.5.
pub type ChunkKey = usize;

/// A 3D sliding window of `Option<ChunkKey>`. Renamed from the C++
/// `ChunkPointerArray`. Index layout: `(local.x * size + local.y) * size +
/// local.z`. Cells outside the window return `None` from [`Self::get`].
/// [`Self::set`] is a no-op for out-of-range coords.
#[derive(Debug, Clone)]
pub struct ChunkGrid {
    size: usize,
    origin: Vec3i,
    cells: Vec<Option<ChunkKey>>,
}

impl ChunkGrid {
    /// Build an empty grid of edge length `size`. Origin starts at `(0,0,0)`.
    #[must_use]
    pub fn new(size: usize) -> Self {
        Self {
            size,
            origin: Vec3i::new(0, 0, 0),
            cells: vec![None; size * size * size],
        }
    }

    #[must_use]
    pub fn size(&self) -> usize {
        self.size
    }

    #[must_use]
    pub fn origin(&self) -> Vec3i {
        self.origin
    }

    /// Local index helper. Returns `None` if `local` falls outside `[0, size)`.
    fn index(&self, local: Vec3i) -> Option<usize> {
        let s = self.size as i32;
        if local.x < 0
            || local.y < 0
            || local.z < 0
            || local.x >= s
            || local.y >= s
            || local.z >= s
        {
            return None;
        }
        let s = self.size;
        Some((local.x as usize * s + local.y as usize) * s + local.z as usize)
    }

    /// True iff `ccoord` is inside the current window.
    #[must_use]
    pub fn contains(&self, ccoord: Vec3i) -> bool {
        self.index(ccoord - self.origin).is_some()
    }

    /// Lookup a chunk by world chunk coord. Returns `None` if outside the
    /// window or if the cell is empty.
    #[must_use]
    pub fn get(&self, ccoord: Vec3i) -> Option<ChunkKey> {
        self.cells[self.index(ccoord - self.origin)?]
    }

    /// Write a cell. No-op if `ccoord` is outside the current window.
    pub fn set(&mut self, ccoord: Vec3i, key: Option<ChunkKey>) {
        if let Some(idx) = self.index(ccoord - self.origin) {
            self.cells[idx] = key;
        }
    }

    /// Slide the window so its lower corner becomes `ccoord`. Cells whose
    /// world coords still fall inside both the old and the new window keep
    /// their values; everything else is dropped. Mirrors the C++
    /// `ChunkPointerArray::set_center` / `move`.
    pub fn set_center(&mut self, ccoord: Vec3i) {
        if ccoord == self.origin {
            return;
        }
        let s = self.size;
        let mut next: Vec<Option<ChunkKey>> = vec![None; s * s * s];
        for x in 0..self.size as i32 {
            for y in 0..self.size as i32 {
                for z in 0..self.size as i32 {
                    let local_new = Vec3i::new(x, y, z);
                    let world = local_new + ccoord;
                    let local_old = world - self.origin;
                    if let Some(idx_old) = self.index(local_old) {
                        let s = self.size;
                        let idx_new =
                            (x as usize * s + y as usize) * s + z as usize;
                        next[idx_new] = self.cells[idx_old];
                    }
                }
            }
        }
        self.cells = next;
        self.origin = ccoord;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn set_get_round_trip() {
        let mut g = ChunkGrid::new(4);
        g.set(Vec3i::new(2, 1, 3), Some(7));
        assert_eq!(g.get(Vec3i::new(2, 1, 3)), Some(7));
        assert_eq!(g.get(Vec3i::new(0, 0, 0)), None);
    }

    #[test]
    fn set_outside_is_no_op_and_get_returns_none() {
        let mut g = ChunkGrid::new(4);
        g.set(Vec3i::new(-1, 0, 0), Some(42));
        g.set(Vec3i::new(4, 0, 0), Some(43));
        assert_eq!(g.get(Vec3i::new(-1, 0, 0)), None);
        assert_eq!(g.get(Vec3i::new(4, 0, 0)), None);
        for x in 0..4 {
            for y in 0..4 {
                for z in 0..4 {
                    assert!(g.get(Vec3i::new(x, y, z)).is_none());
                }
            }
        }
    }

    #[test]
    fn set_center_drops_outside_keeps_overlap() {
        let mut g = ChunkGrid::new(4);
        g.set(Vec3i::new(0, 0, 0), Some(1));
        g.set(Vec3i::new(3, 3, 3), Some(2));
        g.set_center(Vec3i::new(1, 1, 1));
        assert_eq!(g.origin(), Vec3i::new(1, 1, 1));
        assert_eq!(g.get(Vec3i::new(0, 0, 0)), None);
        assert_eq!(g.get(Vec3i::new(3, 3, 3)), Some(2));
        assert_eq!(g.get(Vec3i::new(4, 4, 4)), None);
    }

    #[test]
    fn set_center_preserves_inner_overlap() {
        let mut g = ChunkGrid::new(6);
        for x in 1..=4 {
            for y in 1..=4 {
                for z in 1..=4 {
                    g.set(
                        Vec3i::new(x, y, z),
                        Some((x * 100 + y * 10 + z) as usize),
                    );
                }
            }
        }
        g.set_center(Vec3i::new(2, 2, 2));
        for x in 3..=4 {
            for y in 3..=4 {
                for z in 3..=4 {
                    assert_eq!(
                        g.get(Vec3i::new(x, y, z)),
                        Some((x * 100 + y * 10 + z) as usize),
                        "lost overlap cell ({x},{y},{z})",
                    );
                }
            }
        }
    }
}
