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
use crate::math::Vec3u;

// ----------------------------------------------------------------------
//   Chunk
// ----------------------------------------------------------------------

/// One cache slot in the world's chunk store. Holds the block array
/// under a `parking_lot::RwLock` (wrapped in `Arc` so transactions can
/// take owned guards without lifetime gymnastics) plus the lock-free
/// commit/save/updated atomics.
pub struct Chunk {
    /// Inner `Arc` so callers can take owned `ArcRwLockReadGuard` /
    /// `ArcRwLockWriteGuard` via `parking_lot`'s `arc_lock` feature.
    /// Without the inner `Arc`, an owned guard would need lifetime
    /// gymnastics (or `unsafe`); with it, txn entries hold the guard
    /// directly and don't need to borrow from the `Arc<Chunk>`.
    blocks: Arc<RwLock<ChunkData>>,
    save_gen: AtomicU64,
    commit_gen: AtomicU64,
    updated: AtomicBool,
}

impl Chunk {
    /// `log2(SIZE)` — chunks are `SIZE × SIZE × SIZE` blocks.
    pub const SIZE_LOG: usize = 4;

    /// Edge length of a chunk (= 16).
    pub const SIZE: usize = 1 << Self::SIZE_LOG;

    /// Chunk wrapping blocks that came from disk. `(save_gen,
    /// commit_gen) = (1, 1)` — memory matches disk.
    pub(super) fn from_disk(blocks: ChunkData) -> Self {
        Self::with_gens(blocks, 1, 1)
    }

    /// Chunk wrapping freshly-generated blocks. `(save_gen,
    /// commit_gen) = (0, 1)` — dirty until written out.
    pub(super) fn from_gen(blocks: ChunkData) -> Self {
        Self::with_gens(blocks, 0, 1)
    }

    fn with_gens(blocks: ChunkData, save_gen: u64, commit_gen: u64) -> Self {
        debug_assert!(save_gen <= commit_gen, "save_gen ≤ commit_gen invariant");
        Self {
            blocks: Arc::new(RwLock::new(blocks)),
            save_gen: AtomicU64::new(save_gen),
            commit_gen: AtomicU64::new(commit_gen),
            updated: AtomicBool::new(false),
        }
    }

    /// Take an owned read guard. The guard owns a clone of the inner
    /// `Arc<RwLock<Blocks>>`, so it doesn't borrow from the `Chunk`.
    pub(super) fn read_blocks(&self) -> ArcRwLockReadGuard<RawRwLock, ChunkData> {
        self.blocks.read_arc()
    }

    /// Take an owned write guard. See [`Self::read_blocks`].
    pub(super) fn write_blocks(&self) -> ArcRwLockWriteGuard<RawRwLock, ChunkData> {
        self.blocks.write_arc()
    }

    // ----- save_gen / commit_gen -----

    pub(super) fn commit_gen(&self) -> u64 {
        self.commit_gen.load(Ordering::Acquire)
    }

    #[cfg(test)]
    pub(super) fn save_gen(&self) -> u64 {
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
    pub(super) fn advance_save_gen(&self, captured: u64) {
        self.save_gen.fetch_max(captured, Ordering::AcqRel);
    }

    /// True iff `save_gen < commit_gen` — i.e. there are committed
    /// changes not yet written to disk. Read order: save first, then
    /// commit, so the pair `(A, B)` always satisfies `A ≤ B`.
    pub(super) fn dirty(&self) -> bool {
        let save = self.save_gen.load(Ordering::Acquire);
        let commit = self.commit_gen.load(Ordering::Acquire);
        save < commit
    }

    // ----- updated -----

    pub(super) fn mark_updated(&self) {
        self.updated.store(true, Ordering::Release);
    }

    pub(super) fn clear_updated(&self) {
        self.updated.store(false, Ordering::Release);
    }

    pub(super) fn updated(&self) -> bool {
        self.updated.load(Ordering::Acquire)
    }
}

// ----------------------------------------------------------------------
//   ChunkData
// ----------------------------------------------------------------------

/// The `SIZE × SIZE × SIZE` block array for one chunk. A `Chunk`'s data
/// lives behind a `parking_lot::RwLock<ChunkData>`.
pub struct ChunkData([BlockData; Chunk::SIZE * Chunk::SIZE * Chunk::SIZE]);

impl ChunkData {
    /// Magic bytes (`"NEWC"`) identifying a packaged chunk on disk.
    pub const MAGIC: u32 = 0x4E45_5743;

    /// Current on-disk format version. Version 3 adds zstd compression of
    /// the body — the header stays plain so corrupt files can be
    /// diagnosed without invoking the decoder.
    pub const VERSION: u32 = 3;

    /// Header size in bytes.
    pub const HEADER_SIZE: usize = 8;

    /// Total per-block bytes when serialised to disk.
    pub const DATA_SIZE: usize = Chunk::SIZE * Chunk::SIZE * Chunk::SIZE * BlockData::ENCODED_LEN;

    /// zstd compression level used when packaging a chunk. Level 3 is
    /// zstd's "balanced" default — fast enough for save-on-evict on the
    /// main thread (~tens of µs per chunk on contemporary CPUs) and
    /// gives a strong ratio on homogeneous voxel data.
    pub const COMPRESSION_LEVEL: i32 = 3;

    /// Air-filled blocks at the given default light.
    pub fn air_filled(default_light: Light) -> Self {
        let fill = BlockData {
            id: BlockId::EMPTY,
            state: State::default(),
            light: default_light,
        };
        Self([fill; Chunk::SIZE * Chunk::SIZE * Chunk::SIZE])
    }

    /// Linear index for `(x, y, z)` block-local coords (X-major,
    /// matching the C++ port).
    pub fn index(bcoord: Vec3u) -> usize {
        let x = bcoord.x as usize;
        let y = bcoord.y as usize;
        let z = bcoord.z as usize;
        debug_assert!(
            x < Chunk::SIZE && y < Chunk::SIZE && z < Chunk::SIZE,
            "block coordinates out of bounds"
        );
        (x * Chunk::SIZE + y) * Chunk::SIZE + z
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

    /// Serialize to bytes: a 12-byte plain header followed by the
    /// zstd-compressed block array (`SIZE_CUBED` little-endian
    /// records before compression). `current_to_canonical` translates
    /// each cell's in-memory id to the canonical id stored on disk;
    /// pass an empty slice for an identity mapping.
    ///
    /// Compression typically shrinks chunks 5–200× — homogeneous
    /// chunks (pure air above terrain, pure rock below) collapse
    /// almost to nothing, mixed chunks settle around 4–10×.
    pub fn package_to(&self, current_to_canonical: &[u16]) -> Vec<u8> {
        let mut body = Vec::with_capacity(Self::DATA_SIZE);
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
            translated.encode_to(&mut body);
        }
        let compressed = zstd::bulk::compress(&body, Self::COMPRESSION_LEVEL)
            .expect("zstd compression of a fixed-size buffer cannot fail");
        let mut out = Vec::with_capacity(Self::HEADER_SIZE + compressed.len());
        out.extend_from_slice(&Self::MAGIC.to_le_bytes());
        out.extend_from_slice(&Self::VERSION.to_le_bytes());
        out.extend_from_slice(&compressed);
        out
    }

    /// Deserialize bytes (produced by [`Self::package_to`]) into
    /// `self`. Verifies magic + version, then decompresses the body
    /// directly into a `SIZE_DATA`-bounded buffer (so a malicious
    /// header can't trigger an unbounded allocation).
    /// `canonical_to_current` translates each cell's canonical id
    /// (read from disk) to the in-memory `BlockId`; pass an empty
    /// slice for identity.
    pub fn unpackage_from(
        &mut self,
        bytes: &[u8],
        canonical_to_current: &[BlockId],
    ) -> Result<(), ChunkError> {
        if bytes.len() < Self::HEADER_SIZE {
            return Err(ChunkError::Size {
                expected: Self::HEADER_SIZE,
                got: bytes.len(),
            });
        }
        let (head, body) = bytes.split_at(Self::HEADER_SIZE);
        let magic = u32::from_le_bytes([head[0], head[1], head[2], head[3]]);
        let version = u32::from_le_bytes([head[4], head[5], head[6], head[7]]);
        if magic != Self::MAGIC {
            return Err(ChunkError::Magic);
        }
        if version != Self::VERSION {
            return Err(ChunkError::Version { got: version });
        }
        let mut decompressed = vec![0u8; Self::DATA_SIZE];
        let n = zstd::bulk::decompress_to_buffer(body, &mut decompressed)
            .map_err(|_| ChunkError::Compression)?;
        if n != Self::DATA_SIZE {
            return Err(ChunkError::Size {
                expected: Self::DATA_SIZE,
                got: n,
            });
        }
        let identity = canonical_to_current.is_empty();
        for (i, slot) in self.0.iter_mut().enumerate() {
            let off = i * BlockData::ENCODED_LEN;
            let mut cell = BlockData::decode_from(&decompressed[off..off + BlockData::ENCODED_LEN]);
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

/// Errors returned by [`ChunkData::unpackage_from`].
#[derive(Debug, Error, PartialEq, Eq)]
pub enum ChunkError {
    #[error("chunk header has bad magic")]
    Magic,
    #[error("chunk header has bad version: got {got}")]
    Version { got: u32 },
    #[error("chunk has bad size: expected {expected} bytes, got {got}")]
    Size { expected: usize, got: usize },
    #[error("chunk body failed zstd decompression")]
    Compression,
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
    fn air_filled_blocks_default_light() {
        let base = make_base();
        let blocks = ChunkData::air_filled(Light::SKY);
        let b = blocks.block(Vec3u::new(0, 0, 0));
        assert_eq!(b.id, base.air);
        assert_eq!(b.light, Light::SKY);
    }

    #[test]
    fn block_mut_writes_value() {
        let base = make_base();
        let mut blocks = ChunkData::air_filled(Light::SKY);
        blocks.block_mut(Vec3u::new(2, 3, 4)).id = base.rock;
        assert_eq!(blocks.block(Vec3u::new(2, 3, 4)).id, base.rock);
        assert_eq!(blocks.block(Vec3u::new(0, 0, 0)).id, base.air);
    }

    #[test]
    fn package_round_trips_through_unpackage() {
        let base = make_base();
        let mut original = ChunkData::air_filled(Light::SKY);
        original.block_mut(Vec3u::new(0, 0, 0)).id = base.rock;
        original.block_mut(Vec3u::new(15, 15, 15)).id = base.bedrock;
        original.block_mut(Vec3u::new(8, 4, 2)).id = base.water;
        original.block_mut(Vec3u::new(1, 2, 3)).light = Light::new(7, 11);

        let bytes = original.package_to(&[]);
        // After zstd compression a near-uniform chunk is much
        // smaller than the raw body — strict upper bound only.
        assert!(bytes.len() >= ChunkData::HEADER_SIZE);
        assert!(bytes.len() < ChunkData::HEADER_SIZE + ChunkData::DATA_SIZE);

        let mut loaded = ChunkData::air_filled(Light::SKY);
        loaded.unpackage_from(&bytes, &[]).expect("unpackage");
        assert_eq!(loaded.block(Vec3u::new(0, 0, 0)).id, base.rock);
        assert_eq!(loaded.block(Vec3u::new(15, 15, 15)).id, base.bedrock);
        assert_eq!(loaded.block(Vec3u::new(8, 4, 2)).id, base.water);
        assert_eq!(loaded.block(Vec3u::new(1, 2, 3)).light, Light::new(7, 11));
    }

    #[test]
    fn air_only_chunk_compresses_far_below_raw() {
        // Pure-air chunk: zstd should knock the body down to a
        // handful of bytes. Lock in the win so a future codec
        // regression doesn't silently bloat the database.
        let original = ChunkData::air_filled(Light::SKY);
        let bytes = original.package_to(&[]);
        assert!(
            bytes.len() < ChunkData::HEADER_SIZE + 64,
            "expected pure-air chunk to compress to < 64 bytes after header, got {}",
            bytes.len() - ChunkData::HEADER_SIZE,
        );
    }

    #[test]
    fn unpackage_rejects_bad_magic() {
        let mut blocks = ChunkData::air_filled(Light::SKY);
        let bytes = vec![0_u8; ChunkData::HEADER_SIZE + ChunkData::DATA_SIZE];
        assert_eq!(blocks.unpackage_from(&bytes, &[]), Err(ChunkError::Magic));
    }

    #[test]
    fn unpackage_rejects_bad_version() {
        let mut blocks = ChunkData::air_filled(Light::SKY);
        let mut bytes = vec![0_u8; ChunkData::HEADER_SIZE + ChunkData::DATA_SIZE];
        bytes[0..4].copy_from_slice(&ChunkData::MAGIC.to_le_bytes());
        bytes[4..8].copy_from_slice(&999_u32.to_le_bytes());
        assert_eq!(
            blocks.unpackage_from(&bytes, &[]),
            Err(ChunkError::Version { got: 999 })
        );
    }

    #[test]
    fn from_disk_starts_clean() {
        let p = Chunk::from_disk(ChunkData::air_filled(Light::SKY));
        assert_eq!(p.save_gen(), 1);
        assert_eq!(p.commit_gen(), 1);
        assert!(!p.dirty());
    }

    #[test]
    fn from_gen_starts_dirty() {
        let p = Chunk::from_gen(ChunkData::air_filled(Light::SKY));
        assert_eq!(p.save_gen(), 0);
        assert_eq!(p.commit_gen(), 1);
        assert!(p.dirty());
    }

    #[test]
    fn commit_then_save() {
        let p = Chunk::from_disk(ChunkData::air_filled(Light::SKY));
        p.bump_commit_gen();
        assert!(p.dirty());
        let captured = p.commit_gen();
        p.advance_save_gen(captured);
        assert!(!p.dirty());
    }

    #[test]
    fn second_commit_during_save_keeps_dirty() {
        let p = Chunk::from_disk(ChunkData::air_filled(Light::SKY));
        p.bump_commit_gen();
        let captured = p.commit_gen();
        p.bump_commit_gen();
        p.advance_save_gen(captured);
        assert!(p.dirty());
    }

    #[test]
    fn updated_default_false_set_and_clear() {
        let p = Chunk::from_disk(ChunkData::air_filled(Light::SKY));
        assert!(!p.updated());
        p.mark_updated();
        assert!(p.updated());
        p.clear_updated();
        assert!(!p.updated());
    }
}
