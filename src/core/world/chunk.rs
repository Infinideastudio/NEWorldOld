//! Chunk storage — port of C++ `chunks.ixx::Chunk`.
//!
//! See `docs/rust_migration.md` §4.3. A chunk owns a lazily-allocated
//! `Box<[BlockData; SIZE_CUBED]>` (16³ = 4096 cells) plus three flags:
//! `empty` (no data allocated yet), `updated` (mesh dirty), `modified`
//! (on-disk save dirty).
//!
//! `BaseBlocks` is passed in as an explicit parameter to every accessor that
//! needs the `air` id rather than stored on the chunk: per the migration plan
//! the `BlockRegistry` lives in `Arc<BlockRegistry>` on `GameApp`, not on
//! individual chunks, and the C++ globals (`base_blocks()`, `block_info_registry`)
//! are deliberately not ported.

use thiserror::Error;

use crate::blocks::{BaseBlocks, BlockData, BlockId, Light, State};
use crate::math::{Aabbd, Vec3d, Vec3i, Vec3u};

// ---------- Save format header ----------

/// Magic bytes (`"NEWC"`) identifying a packaged chunk.
const MAGIC: u32 = 0x4E45_5743;
/// Current on-disk format version. Bump on any layout change to the body.
///
/// Version 2 is the current format: 12-byte header + per-cell little-endian
/// encoding (`id u16 + state u16 + light u8`) = 5 bytes per cell. The id
/// numbers are *world-canonical*: a per-world id↔name mapping (saved in
/// `world.dat`) is consulted by the loader to translate to current-registry
/// ids. See [`Chunk::unpackage_from`].
const VERSION: u32 = 2;
/// Reserved-for-future-use flags field; written as zero.
const FLAGS: u32 = 0;
/// Header size in bytes.
const HEADER_SIZE: usize = 12;

// ---------- ChunkError ----------

/// Errors returned by [`Chunk::unpackage_from`].
#[derive(Debug, Error, PartialEq, Eq)]
pub enum ChunkError {
    /// File header magic does not match `"NEWC"`.
    #[error("chunk header has bad magic")]
    BadMagic,
    /// File header version is not understood by this build.
    #[error("chunk header has bad version: got {got}")]
    BadVersion { got: u32 },
    /// Body size after the header doesn't match `SIZE_DATA`.
    #[error("chunk has bad size: expected {expected} bytes, got {got}")]
    BadSize { expected: usize, got: usize },
}

// ---------- Chunk ----------

/// A `SIZE × SIZE × SIZE` voxel chunk at integer chunk coord `coord`.
///
/// Block data is heap-allocated lazily on first write — empty chunks (e.g.
/// pure-air sky chunks) take no per-cell memory until they're modified.
/// Emptiness is encoded *in* `data`: the invariant `empty() ⇔ data.is_none()`
/// is maintained by every method that touches `data`. There is no separate
/// `empty` flag to keep in sync.
pub struct Chunk {
    pub(super) coord: Vec3i,
    pub(super) data: Option<Box<[BlockData; Self::SIZE_CUBED]>>,
    updated: bool,
    modified: bool,
}

impl Chunk {
    /// `log2(SIZE)` — chunks are `SIZE × SIZE × SIZE` cells.
    pub const SIZE_LOG: i32 = 4;
    /// Edge length of a chunk (= `1 << SIZE_LOG` = 16).
    pub const SIZE: i32 = 1 << Self::SIZE_LOG;
    /// `SIZE` as a `usize` for indexing.
    pub const SIZE_USIZE: usize = Self::SIZE as usize;
    /// Total cells per chunk (= 4096).
    pub const SIZE_CUBED: usize = Self::SIZE_USIZE * Self::SIZE_USIZE * Self::SIZE_USIZE;
    /// Total per-cell data size in bytes (`SIZE_CUBED * BlockData::ENCODED_LEN`).
    pub const SIZE_DATA: usize = Self::SIZE_CUBED * BlockData::ENCODED_LEN;

    /// Empty (no allocated data) chunk at integer chunk coord `coord`.
    #[must_use]
    pub fn new(coord: Vec3i) -> Self {
        Self {
            coord,
            data: None,
            updated: false,
            modified: false,
        }
    }

    /// Integer chunk coordinate (multiply by `SIZE` to get world coords).
    #[must_use]
    pub fn coord(&self) -> Vec3i {
        self.coord
    }

    /// True iff no block data has been allocated yet — the chunk reads as
    /// uniform air with sky/no-light derived from `coord.y`. Equivalent to
    /// `self.data.is_none()` (the field is the canonical encoding of
    /// emptiness; see the struct doc).
    #[must_use]
    pub fn empty(&self) -> bool {
        self.data.is_none()
    }

    /// True iff the render mesh is dirty.
    #[must_use]
    pub fn updated(&self) -> bool {
        self.updated
    }

    /// True iff the on-disk save is dirty.
    #[must_use]
    pub fn modified(&self) -> bool {
        self.modified
    }

    /// Clear the "save is dirty" flag. Call after a successful `save`.
    pub fn clear_modified(&mut self) {
        self.modified = false;
    }

    /// Clear the "mesh is dirty" flag. Call after a successful re-mesh.
    pub fn clear_updated(&mut self) {
        self.updated = false;
    }

    /// Mark this chunk as needing a re-mesh because an adjacent chunk's edge
    /// changed. Mirrors the C++ `mark_neighbor_updated`.
    pub fn mark_neighbor_updated(&mut self) {
        self.updated = true;
    }

    /// Axis-aligned world-space bounding box `[coord*SIZE, coord*SIZE+SIZE]`.
    #[must_use]
    pub fn aabb(&self) -> Aabbd {
        let lo = self.coord * Self::SIZE;
        let hi = lo + Vec3i::new(Self::SIZE, Self::SIZE, Self::SIZE);
        Aabbd::new(
            Vec3d::new(f64::from(lo.x), f64::from(lo.y), f64::from(lo.z)),
            Vec3d::new(f64::from(hi.x), f64::from(hi.y), f64::from(hi.z)),
        )
    }

    /// Default light value for empty / freshly-allocated chunks: full sky if
    /// at or above y=0, total darkness underground. Mirrors the C++
    /// `_coord.y() < 0 ? NO_LIGHT : SKY_LIGHT` formula (the C++ code
    /// branches on the chunk-coord, not the world-coord).
    fn default_light(&self) -> Light {
        if self.coord.y < 0 {
            Light::NONE
        } else {
            Light::SKY
        }
    }

    /// Linear index into the data array for `(x, y, z)` (X-major, matching
    /// C++).
    #[inline]
    fn index(bcoord: Vec3u) -> usize {
        let x = bcoord.x as usize;
        let y = bcoord.y as usize;
        let z = bcoord.z as usize;
        debug_assert!(
            x < Self::SIZE_USIZE && y < Self::SIZE_USIZE && z < Self::SIZE_USIZE,
            "block coordinates out of bounds"
        );
        (x * Self::SIZE_USIZE + y) * Self::SIZE_USIZE + z
    }

    /// Read one block. For empty chunks returns
    /// `BlockData { id: base.air, light: default_light(), state: 0 }`.
    #[must_use]
    pub fn block(&self, bcoord: Vec3u, base: &BaseBlocks) -> BlockData {
        assert!(
            bcoord.x < Self::SIZE as u32
                && bcoord.y < Self::SIZE as u32
                && bcoord.z < Self::SIZE as u32,
            "block coordinates out of bounds"
        );
        if let Some(data) = &self.data {
            data[Self::index(bcoord)]
        } else {
            BlockData {
                id: base.air,
                state: State::default(),
                light: self.default_light(),
            }
        }
    }

    /// Write-access to one block. Lazily allocates the data array (filling
    /// with air at the chunk's default light) on first call, then flips
    /// `updated = true`, `modified = true`. Allocation alone makes the
    /// chunk non-empty under the `empty() ⇔ data.is_none()` invariant — no
    /// separate `empty` flag to update.
    pub fn block_mut(&mut self, bcoord: Vec3u, base: &BaseBlocks) -> &mut BlockData {
        assert!(
            bcoord.x < Self::SIZE as u32
                && bcoord.y < Self::SIZE as u32
                && bcoord.z < Self::SIZE as u32,
            "block coordinates out of bounds"
        );
        if self.data.is_none() {
            let fill = BlockData {
                id: base.air,
                state: State(0),
                light: self.default_light(),
            };
            self.data = Some(Box::new([fill; Self::SIZE_CUBED]));
        }
        self.updated = true;
        self.modified = true;
        let data = self
            .data
            .as_mut()
            .expect("data is Some after lazy allocation");
        &mut data[Self::index(bcoord)]
    }

    /// If `data` is None, allocate it without changing the empty/updated/modified
    /// flags. Used internally by terrain generation.
    pub(super) fn ensure_data(&mut self) {
        if self.data.is_none() {
            self.data = Some(Box::new([BlockData::default(); Self::SIZE_CUBED]));
        }
    }

    /// Direct linear-index access into the data array, asserting it's been
    /// allocated. Internal helper for terrain generation.
    pub(super) fn cell_mut(&mut self, idx: usize) -> &mut BlockData {
        self.data
            .as_mut()
            .expect("ensure_data called before cell_mut")
            .as_mut()
            .get_mut(idx)
            .expect("index in bounds")
    }

    // ---------- Save / load ----------

    /// Serialize this chunk to bytes: a `[magic, version, flags]` header
    /// followed by `SIZE_CUBED` little-endian cells. Returns an empty `Vec`
    /// for empty chunks (callers should not save empty chunks).
    ///
    /// `current_to_canonical` translates each cell's *current-registry* id
    /// to the *world-canonical* id stored on disk (see [`Self::unpackage_from`]).
    /// Indexed by current registry id; any in-memory id whose entry exceeds
    /// the slice length, or maps to `u16::MAX`, is encoded as
    /// [`BlockId::EMPTY`] on disk. Pass an empty slice for an identity
    /// mapping (every id is written verbatim) — used when the world's
    /// canonical mapping equals the current registry, or by tests.
    #[must_use]
    pub fn package_to(&self, current_to_canonical: &[u16]) -> Vec<u8> {
        let Some(data) = &self.data else {
            return Vec::new();
        };
        let mut out = Vec::with_capacity(HEADER_SIZE + Self::SIZE_DATA);
        out.extend_from_slice(&MAGIC.to_le_bytes());
        out.extend_from_slice(&VERSION.to_le_bytes());
        out.extend_from_slice(&FLAGS.to_le_bytes());
        let identity = current_to_canonical.is_empty();
        for cell in data.iter() {
            let mut translated = *cell;
            if !identity {
                let canonical = current_to_canonical
                    .get(cell.id.0 as usize)
                    .copied()
                    .unwrap_or(u16::MAX);
                translated.id = if canonical == u16::MAX {
                    BlockId::EMPTY
                } else {
                    BlockId(canonical)
                };
            }
            translated.encode_to(&mut out);
        }
        out
    }

    /// Deserialize a chunk from bytes produced by [`Self::package_to`].
    /// Verifies magic and version, then decodes the body into the data
    /// array (allocating it if necessary). Allocation alone makes `empty()`
    /// read false; sets `modified = false` (just loaded from disk; not
    /// dirty).
    ///
    /// `canonical_to_current` translates each cell's *world-canonical* id
    /// (read from disk) to the *current-registry* id used in memory.
    /// Indexed by canonical id; any saved id whose entry exceeds the slice
    /// length is treated as [`BlockId::EMPTY`] (the block is "missing"
    /// from the current registry — typically a removed mod). Pass an empty
    /// slice for an identity mapping.
    pub fn unpackage_from(
        &mut self,
        bytes: &[u8],
        canonical_to_current: &[BlockId],
    ) -> Result<(), ChunkError> {
        if bytes.len() < HEADER_SIZE {
            return Err(ChunkError::BadSize {
                expected: HEADER_SIZE + Self::SIZE_DATA,
                got: bytes.len(),
            });
        }
        let (head, body) = bytes.split_at(HEADER_SIZE);
        let magic = u32::from_le_bytes([head[0], head[1], head[2], head[3]]);
        let version = u32::from_le_bytes([head[4], head[5], head[6], head[7]]);
        // head[8..12] is the reserved flags field; ignored on read.
        if magic != MAGIC {
            return Err(ChunkError::BadMagic);
        }
        if version != VERSION {
            return Err(ChunkError::BadVersion { got: version });
        }
        if body.len() != Self::SIZE_DATA {
            return Err(ChunkError::BadSize {
                expected: Self::SIZE_DATA,
                got: body.len(),
            });
        }
        self.ensure_data();
        let data = self.data.as_mut().expect("ensure_data allocated");
        let identity = canonical_to_current.is_empty();
        for (i, slot) in data.iter_mut().enumerate() {
            let off = i * BlockData::ENCODED_LEN;
            let mut cell = BlockData::decode_from(&body[off..off + BlockData::ENCODED_LEN]);
            if !identity {
                cell.id = canonical_to_current
                    .get(cell.id.0 as usize)
                    .copied()
                    .unwrap_or(BlockId::EMPTY);
            }
            *slot = cell;
        }
        self.modified = false;
        Ok(())
    }
}

// ---------- Tests ----------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{BlockRegistry, register_base_blocks};

    fn make_base() -> BaseBlocks {
        let mut reg = BlockRegistry::new();
        register_base_blocks(&mut reg)
    }

    #[test]
    fn sizes_match_constants() {
        assert_eq!(Chunk::SIZE, 16);
        assert_eq!(Chunk::SIZE_USIZE, 16);
        assert_eq!(Chunk::SIZE_CUBED, 4096);
        assert_eq!(Chunk::SIZE_DATA, 4096 * BlockData::ENCODED_LEN);
    }

    #[test]
    fn empty_chunk_returns_air_with_sky_light_above_zero() {
        let base = make_base();
        let c = Chunk::new(Vec3i::new(0, 5, 0));
        assert!(c.empty());
        assert!(!c.updated());
        assert!(!c.modified());
        let b = c.block(Vec3u::new(0, 0, 0), &base);
        assert_eq!(b.id, base.air);
        assert_eq!(b.light, Light::SKY);
    }

    #[test]
    fn empty_chunk_returns_air_with_no_light_below_zero() {
        let base = make_base();
        let c = Chunk::new(Vec3i::new(0, -1, 0));
        let b = c.block(Vec3u::new(3, 7, 11), &base);
        assert_eq!(b.id, base.air);
        assert_eq!(b.light, Light::NONE);
    }

    #[test]
    fn block_mut_lazily_allocates_and_flips_flags() {
        let base = make_base();
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        assert!(c.empty());
        assert!(!c.updated());
        assert!(!c.modified());
        {
            let cell = c.block_mut(Vec3u::new(2, 3, 4), &base);
            // Default fill is air with sky-light (since coord.y == 0 → SKY).
            assert_eq!(cell.id, base.air);
            assert_eq!(cell.light, Light::SKY);
            cell.id = base.rock;
        }
        assert!(!c.empty());
        assert!(c.updated());
        assert!(c.modified());
        // Read back through the immutable accessor.
        assert_eq!(c.block(Vec3u::new(2, 3, 4), &base).id, base.rock);
        // Other cells are still air.
        assert_eq!(c.block(Vec3u::new(0, 0, 0), &base).id, base.air);
    }

    #[test]
    fn aabb_is_coord_times_size_box() {
        let c = Chunk::new(Vec3i::new(2, -1, 3));
        let bb = c.aabb();
        assert_eq!(bb.min, Vec3d::new(32.0, -16.0, 48.0));
        assert_eq!(bb.max, Vec3d::new(48.0, 0.0, 64.0));
    }

    #[test]
    fn mark_neighbor_updated_sets_updated() {
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        assert!(!c.updated());
        c.mark_neighbor_updated();
        assert!(c.updated());
    }

    #[test]
    fn clear_flags() {
        let base = make_base();
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        c.block_mut(Vec3u::new(0, 0, 0), &base).id = base.rock;
        assert!(c.updated());
        assert!(c.modified());
        c.clear_updated();
        c.clear_modified();
        assert!(!c.updated());
        assert!(!c.modified());
    }

    #[test]
    fn package_round_trips_through_unpackage() {
        let base = make_base();
        let mut original = Chunk::new(Vec3i::new(7, 2, -3));
        // Plant a few distinct blocks across the chunk.
        original.block_mut(Vec3u::new(0, 0, 0), &base).id = base.rock;
        original.block_mut(Vec3u::new(15, 15, 15), &base).id = base.bedrock;
        original.block_mut(Vec3u::new(8, 4, 2), &base).id = base.water;
        // Set a non-default light on one cell, too.
        original.block_mut(Vec3u::new(1, 2, 3), &base).light = Light::new(7, 11);

        let bytes = original.package_to(&[]);
        assert_eq!(bytes.len(), HEADER_SIZE + Chunk::SIZE_DATA);

        let mut loaded = Chunk::new(Vec3i::new(7, 2, -3));
        loaded
            .unpackage_from(&bytes, &[])
            .expect("unpackage should succeed");
        assert!(!loaded.empty());
        assert!(!loaded.modified()); // freshly loaded
        assert_eq!(loaded.block(Vec3u::new(0, 0, 0), &base).id, base.rock);
        assert_eq!(loaded.block(Vec3u::new(15, 15, 15), &base).id, base.bedrock);
        assert_eq!(loaded.block(Vec3u::new(8, 4, 2), &base).id, base.water);
        assert_eq!(
            loaded.block(Vec3u::new(1, 2, 3), &base).light,
            Light::new(7, 11)
        );
        // Sample untouched cells: should still be air (the lazy-fill default).
        assert_eq!(loaded.block(Vec3u::new(5, 5, 5), &base).id, base.air);
    }

    #[test]
    fn package_to_returns_empty_for_empty_chunk() {
        let c = Chunk::new(Vec3i::new(0, 0, 0));
        assert!(c.package_to(&[]).is_empty());
    }

    #[test]
    fn unpackage_rejects_bad_magic() {
        let mut bytes = vec![0_u8; HEADER_SIZE + Chunk::SIZE_DATA];
        // Magic is the first 4 bytes — leave them as zero (≠ MAGIC).
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        assert_eq!(c.unpackage_from(&bytes, &[]), Err(ChunkError::BadMagic));
        // Also verify a body of the right size with garbage magic fails the same way.
        bytes[..4].copy_from_slice(&0xDEAD_BEEF_u32.to_le_bytes());
        assert_eq!(c.unpackage_from(&bytes, &[]), Err(ChunkError::BadMagic));
    }

    #[test]
    fn unpackage_rejects_bad_version() {
        let mut bytes = vec![0_u8; HEADER_SIZE + Chunk::SIZE_DATA];
        bytes[0..4].copy_from_slice(&MAGIC.to_le_bytes());
        bytes[4..8].copy_from_slice(&999_u32.to_le_bytes());
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        assert_eq!(
            c.unpackage_from(&bytes, &[]),
            Err(ChunkError::BadVersion { got: 999 })
        );
    }

    #[test]
    fn unpackage_rejects_bad_size() {
        // Header-only buffer (no body) — too small.
        let mut bytes = vec![0_u8; HEADER_SIZE];
        bytes[0..4].copy_from_slice(&MAGIC.to_le_bytes());
        bytes[4..8].copy_from_slice(&VERSION.to_le_bytes());
        let mut c = Chunk::new(Vec3i::new(0, 0, 0));
        match c.unpackage_from(&bytes, &[]) {
            Err(ChunkError::BadSize { expected: _, got }) => {
                assert_eq!(got, 0);
            }
            other => panic!("expected BadSize, got {other:?}"),
        }

        // Truncated buffer (header valid, body too short).
        let mut short = vec![0_u8; HEADER_SIZE + Chunk::SIZE_DATA - 1];
        short[0..4].copy_from_slice(&MAGIC.to_le_bytes());
        short[4..8].copy_from_slice(&VERSION.to_le_bytes());
        match c.unpackage_from(&short, &[]) {
            Err(ChunkError::BadSize { expected, got }) => {
                assert_eq!(expected, Chunk::SIZE_DATA);
                assert_eq!(got, Chunk::SIZE_DATA - 1);
            }
            other => panic!("expected BadSize, got {other:?}"),
        }

        // Way-too-small buffer (less than the header itself).
        let tiny = vec![0_u8; 4];
        match c.unpackage_from(&tiny, &[]) {
            Err(ChunkError::BadSize { .. }) => {}
            other => panic!("expected BadSize, got {other:?}"),
        }
    }
}
