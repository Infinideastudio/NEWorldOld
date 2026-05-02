//! `Chunk` — one slot in the world's chunk store.
//!
//! Holds a `SIZE × SIZE × SIZE` voxel array under a lock plus the
//! lock-free atomics the eviction / writeback / 2PL machinery consults
//! without locking the data:
//!
//! - `commit_gen` — bumped under the write lock on every commit.
//! - `save_gen`   — advanced (via `fetch_max`) by writeback after a
//!   successful disk write. The pair `(save_gen, commit_gen)` encodes
//!   "is this dirty?" — see [`Self::dirty`].
//! - `updated`    — mesh-rebuild signal consumed by the renderer.
//!
//! There is no per-chunk pin counter: residency is owned by whoever
//! holds a strong [`Arc<Chunk>`]. The world's chunk map drops its
//! `Arc` on eviction; outstanding [`crate::core::world::ReadTxn`] /
//! [`crate::core::world::WriteTxn`] entries hold their own clones, so
//! the chunk lives until the last txn drops.
//!
//! ## save / commit gens
//!
//! Two monotonically-increasing `AtomicU64`s with the invariant
//! `save_gen ≤ commit_gen` at every moment. `commit_gen` is bumped
//! under the write lock during commit, so any reader who re-acquires
//! the chunk lock sees data + bumped gen consistently. `save_gen` is
//! advanced via `fetch_max(captured_gen)` after a successful disk
//! write — monotonic and idempotent.
//!
//! Construction:
//! - [`Chunk::from_disk`] — chunk loaded from sled. `(save_gen,
//!   commit_gen) = (1, 1)` (memory matches disk).
//! - [`Chunk::from_gen`] — chunk just produced by terrain generation.
//!   `(save_gen, commit_gen) = (0, 1)` (no on-disk copy yet; dirty by
//!   definition until written out).
//!
//! `save_gen == 0` means "this coord has no persisted copy" — subsumes
//! what an `on_disk` flag would say.

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};

use parking_lot::{ArcRwLockReadGuard, ArcRwLockWriteGuard, RawRwLock, RwLock};
use thiserror::Error;

use crate::blocks::{BlockData, BlockId, Light, State};
use crate::math::{Vec3i, Vec3u};

// ----------------------------------------------------------------------
//   Constants
// ----------------------------------------------------------------------

/// `log2(SIZE)` — chunks are `SIZE × SIZE × SIZE` blocks.
pub const SIZE_LOG: i32 = 4;
/// Edge length of a chunk (= 16).
pub const SIZE: i32 = 1 << SIZE_LOG;
/// `SIZE` as a `usize` for indexing.
pub const SIZE_USIZE: usize = SIZE as usize;
/// Total blocks per chunk (= 4096).
pub const SIZE_CUBED: usize = SIZE_USIZE * SIZE_USIZE * SIZE_USIZE;
/// Total per-block bytes when serialised to disk.
pub const SIZE_DATA: usize = SIZE_CUBED * BlockData::ENCODED_LEN;

// ----------------------------------------------------------------------
//   On-disk codec — magic / version / errors
// ----------------------------------------------------------------------

/// Magic bytes (`"NEWC"`) identifying a packaged chunk on disk.
const MAGIC: u32 = 0x4E45_5743;
/// Current on-disk format version.
const VERSION: u32 = 2;
/// Reserved-for-future-use flags field; written as zero.
const FLAGS: u32 = 0;
/// Header size in bytes.
const HEADER_SIZE: usize = 12;

/// Errors returned by [`Blocks::unpackage_from`].
#[derive(Debug, Error, PartialEq, Eq)]
pub enum ChunkError {
    #[error("chunk header has bad magic")]
    BadMagic,
    #[error("chunk header has bad version: got {got}")]
    BadVersion { got: u32 },
    #[error("chunk has bad size: expected {expected} bytes, got {got}")]
    BadSize { expected: usize, got: usize },
}

// ----------------------------------------------------------------------
//   Blocks
// ----------------------------------------------------------------------

/// The `SIZE × SIZE × SIZE` block array for one chunk. A `Chunk`'s data
/// lives behind a `parking_lot::RwLock<Blocks>`.
pub struct Blocks([BlockData; SIZE_CUBED]);

impl Blocks {
    /// Air-filled blocks at the given default light.
    pub fn air_filled(default_light: Light) -> Self {
        let fill = BlockData {
            id: BlockId::EMPTY,
            state: State::default(),
            light: default_light,
        };
        Self([fill; SIZE_CUBED])
    }

    /// Linear index for `(x, y, z)` block-local coords (X-major,
    /// matching the C++ port).
    pub fn index(bcoord: Vec3u) -> usize {
        let x = bcoord.x as usize;
        let y = bcoord.y as usize;
        let z = bcoord.z as usize;
        debug_assert!(
            x < SIZE_USIZE && y < SIZE_USIZE && z < SIZE_USIZE,
            "block coordinates out of bounds"
        );
        (x * SIZE_USIZE + y) * SIZE_USIZE + z
    }

    /// Read one block.
    pub fn block(&self, bcoord: Vec3u) -> BlockData {
        self.0[Self::index(bcoord)]
    }

    /// Mutable cell access.
    pub fn block_mut(&mut self, bcoord: Vec3u) -> &mut BlockData {
        &mut self.0[Self::index(bcoord)]
    }

    /// Iterate every cell mutably.
    pub fn iter_mut(&mut self) -> std::slice::IterMut<'_, BlockData> {
        self.0.iter_mut()
    }

    /// Borrow the raw cell slice (read-only).
    pub fn as_slice(&self) -> &[BlockData] {
        &self.0
    }

    /// Serialize to bytes: a 12-byte header + `SIZE_CUBED` little-endian
    /// blocks. `current_to_canonical` translates each cell's in-memory
    /// id to the canonical id stored on disk; pass an empty slice for
    /// an identity mapping.
    pub fn package_to(&self, current_to_canonical: &[u16]) -> Vec<u8> {
        let mut out = Vec::with_capacity(HEADER_SIZE + SIZE_DATA);
        out.extend_from_slice(&MAGIC.to_le_bytes());
        out.extend_from_slice(&VERSION.to_le_bytes());
        out.extend_from_slice(&FLAGS.to_le_bytes());
        let identity = current_to_canonical.is_empty();
        for cell in self.0.iter() {
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

    /// Deserialize bytes (produced by [`Self::package_to`]) into `self`.
    /// Verifies magic + version. `canonical_to_current` translates each
    /// cell's canonical id (read from disk) to the in-memory `BlockId`;
    /// pass an empty slice for identity.
    pub fn unpackage_from(
        &mut self,
        bytes: &[u8],
        canonical_to_current: &[BlockId],
    ) -> Result<(), ChunkError> {
        if bytes.len() < HEADER_SIZE {
            return Err(ChunkError::BadSize {
                expected: HEADER_SIZE + SIZE_DATA,
                got: bytes.len(),
            });
        }
        let (head, body) = bytes.split_at(HEADER_SIZE);
        let magic = u32::from_le_bytes([head[0], head[1], head[2], head[3]]);
        let version = u32::from_le_bytes([head[4], head[5], head[6], head[7]]);
        if magic != MAGIC {
            return Err(ChunkError::BadMagic);
        }
        if version != VERSION {
            return Err(ChunkError::BadVersion { got: version });
        }
        if body.len() != SIZE_DATA {
            return Err(ChunkError::BadSize {
                expected: SIZE_DATA,
                got: body.len(),
            });
        }
        let identity = canonical_to_current.is_empty();
        for (i, slot) in self.0.iter_mut().enumerate() {
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
        Ok(())
    }
}

// ----------------------------------------------------------------------
//   Chunk
// ----------------------------------------------------------------------

/// One cache slot in the world's chunk store. Holds the block array
/// under a `parking_lot::RwLock` (wrapped in `Arc` so transactions can
/// take owned guards without lifetime gymnastics) plus the lock-free
/// commit/save/updated atomics.
pub struct Chunk {
    coord: Vec3i,
    /// Inner `Arc` so callers can take owned `ArcRwLockReadGuard` /
    /// `ArcRwLockWriteGuard` via `parking_lot`'s `arc_lock` feature.
    /// Without the inner `Arc`, an owned guard would need lifetime
    /// gymnastics (or `unsafe`); with it, txn entries hold the guard
    /// directly and don't need to borrow from the `Arc<Chunk>`.
    blocks: Arc<RwLock<Blocks>>,
    save_gen: AtomicU64,
    commit_gen: AtomicU64,
    updated: AtomicBool,
}

impl Chunk {
    /// Chunk wrapping blocks that came from disk. `(save_gen,
    /// commit_gen) = (1, 1)` — memory matches disk.
    pub fn from_disk(coord: Vec3i, blocks: Blocks) -> Self {
        Self::with_gens(coord, blocks, 1, 1)
    }

    /// Chunk wrapping freshly-generated blocks. `(save_gen,
    /// commit_gen) = (0, 1)` — dirty until written out.
    pub fn from_gen(coord: Vec3i, blocks: Blocks) -> Self {
        Self::with_gens(coord, blocks, 0, 1)
    }

    fn with_gens(coord: Vec3i, blocks: Blocks, save_gen: u64, commit_gen: u64) -> Self {
        debug_assert!(save_gen <= commit_gen, "save_gen ≤ commit_gen invariant");
        Self {
            coord,
            blocks: Arc::new(RwLock::new(blocks)),
            save_gen: AtomicU64::new(save_gen),
            commit_gen: AtomicU64::new(commit_gen),
            updated: AtomicBool::new(false),
        }
    }

    pub fn coord(&self) -> Vec3i {
        self.coord
    }

    /// Take an owned read guard. The guard owns a clone of the inner
    /// `Arc<RwLock<Blocks>>`, so it doesn't borrow from the `Chunk`.
    pub fn read_blocks(&self) -> ArcRwLockReadGuard<RawRwLock, Blocks> {
        self.blocks.read_arc()
    }

    /// Take an owned write guard. See [`Self::read_blocks`].
    pub fn write_blocks(&self) -> ArcRwLockWriteGuard<RawRwLock, Blocks> {
        self.blocks.write_arc()
    }

    // ----- save_gen / commit_gen -----

    pub fn commit_gen(&self) -> u64 {
        self.commit_gen.load(Ordering::Acquire)
    }

    pub fn save_gen(&self) -> u64 {
        self.save_gen.load(Ordering::Acquire)
    }

    /// Bump the commit generation. Called by `WriteTxn::commit` after
    /// applying writes, while the write lock is still held.
    pub(super) fn bump_commit_gen(&self) {
        self.commit_gen.fetch_add(1, Ordering::AcqRel);
    }

    /// Advance `save_gen` to `max(save_gen, captured)`. Called by
    /// writeback after a successful disk write. Monotonic and
    /// idempotent.
    pub fn advance_save_gen(&self, captured: u64) {
        self.save_gen.fetch_max(captured, Ordering::AcqRel);
    }

    /// True iff `save_gen < commit_gen` — i.e. there are committed
    /// changes not yet written to disk. Read order: save first, then
    /// commit, so the pair `(A, B)` always satisfies `A ≤ B`.
    pub fn dirty(&self) -> bool {
        let save = self.save_gen.load(Ordering::Acquire);
        let commit = self.commit_gen.load(Ordering::Acquire);
        save < commit
    }

    // ----- updated -----

    pub fn mark_updated(&self) {
        self.updated.store(true, Ordering::Release);
    }

    pub fn clear_updated(&self) {
        self.updated.store(false, Ordering::Release);
    }

    pub fn updated(&self) -> bool {
        self.updated.load(Ordering::Acquire)
    }
}

// ----------------------------------------------------------------------
//   Tests
// ----------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{BaseBlocks, BlockRegistry, register_base_blocks};

    fn make_base() -> BaseBlocks {
        let mut reg = BlockRegistry::new();
        register_base_blocks(&mut reg)
    }

    #[test]
    fn sizes_match_constants() {
        assert_eq!(SIZE, 16);
        assert_eq!(SIZE_USIZE, 16);
        assert_eq!(SIZE_CUBED, 4096);
        assert_eq!(SIZE_DATA, 4096 * BlockData::ENCODED_LEN);
    }

    #[test]
    fn air_filled_blocks_default_light() {
        let base = make_base();
        let blocks = Blocks::air_filled(Light::SKY);
        let b = blocks.block(Vec3u::new(0, 0, 0));
        assert_eq!(b.id, base.air);
        assert_eq!(b.light, Light::SKY);
    }

    #[test]
    fn block_mut_writes_value() {
        let base = make_base();
        let mut blocks = Blocks::air_filled(Light::SKY);
        blocks.block_mut(Vec3u::new(2, 3, 4)).id = base.rock;
        assert_eq!(blocks.block(Vec3u::new(2, 3, 4)).id, base.rock);
        assert_eq!(blocks.block(Vec3u::new(0, 0, 0)).id, base.air);
    }

    #[test]
    fn package_round_trips_through_unpackage() {
        let base = make_base();
        let mut original = Blocks::air_filled(Light::SKY);
        original.block_mut(Vec3u::new(0, 0, 0)).id = base.rock;
        original.block_mut(Vec3u::new(15, 15, 15)).id = base.bedrock;
        original.block_mut(Vec3u::new(8, 4, 2)).id = base.water;
        original.block_mut(Vec3u::new(1, 2, 3)).light = Light::new(7, 11);

        let bytes = original.package_to(&[]);
        assert_eq!(bytes.len(), HEADER_SIZE + SIZE_DATA);

        let mut loaded = Blocks::air_filled(Light::SKY);
        loaded.unpackage_from(&bytes, &[]).expect("unpackage");
        assert_eq!(loaded.block(Vec3u::new(0, 0, 0)).id, base.rock);
        assert_eq!(loaded.block(Vec3u::new(15, 15, 15)).id, base.bedrock);
        assert_eq!(loaded.block(Vec3u::new(8, 4, 2)).id, base.water);
        assert_eq!(loaded.block(Vec3u::new(1, 2, 3)).light, Light::new(7, 11));
    }

    #[test]
    fn unpackage_rejects_bad_magic() {
        let mut blocks = Blocks::air_filled(Light::SKY);
        let bytes = vec![0_u8; HEADER_SIZE + SIZE_DATA];
        assert_eq!(
            blocks.unpackage_from(&bytes, &[]),
            Err(ChunkError::BadMagic)
        );
    }

    #[test]
    fn unpackage_rejects_bad_version() {
        let mut blocks = Blocks::air_filled(Light::SKY);
        let mut bytes = vec![0_u8; HEADER_SIZE + SIZE_DATA];
        bytes[0..4].copy_from_slice(&MAGIC.to_le_bytes());
        bytes[4..8].copy_from_slice(&999_u32.to_le_bytes());
        assert_eq!(
            blocks.unpackage_from(&bytes, &[]),
            Err(ChunkError::BadVersion { got: 999 })
        );
    }

    #[test]
    fn from_disk_starts_clean() {
        let p = Chunk::from_disk(Vec3i::new(0, 0, 0), Blocks::air_filled(Light::SKY));
        assert_eq!(p.save_gen(), 1);
        assert_eq!(p.commit_gen(), 1);
        assert!(!p.dirty());
    }

    #[test]
    fn from_gen_starts_dirty() {
        let p = Chunk::from_gen(Vec3i::new(0, 0, 0), Blocks::air_filled(Light::SKY));
        assert_eq!(p.save_gen(), 0);
        assert_eq!(p.commit_gen(), 1);
        assert!(p.dirty());
    }

    #[test]
    fn commit_then_save() {
        let p = Chunk::from_disk(Vec3i::new(0, 0, 0), Blocks::air_filled(Light::SKY));
        p.bump_commit_gen();
        assert!(p.dirty());
        let captured = p.commit_gen();
        p.advance_save_gen(captured);
        assert!(!p.dirty());
    }

    #[test]
    fn second_commit_during_save_keeps_dirty() {
        let p = Chunk::from_disk(Vec3i::new(0, 0, 0), Blocks::air_filled(Light::SKY));
        p.bump_commit_gen();
        let captured = p.commit_gen();
        p.bump_commit_gen();
        p.advance_save_gen(captured);
        assert!(p.dirty());
    }

    #[test]
    fn updated_default_false_set_and_clear() {
        let p = Chunk::from_disk(Vec3i::new(0, 0, 0), Blocks::air_filled(Light::SKY));
        assert!(!p.updated());
        p.mark_updated();
        assert!(p.updated());
        p.clear_updated();
        assert!(!p.updated());
    }
}
