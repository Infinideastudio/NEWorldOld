//! `WorkingSet`, `ReadTxn`, `WriteTxn` — transactional access to the
//! chunk store.
//!
//! All transactions declare a *working set* (one chunk, an explicit
//! list, or an axis-aligned chunk-coord box). At begin time the txn
//! captures an `Arc<Chunk>` for every chunk in its working set (the
//! Arc itself is the pin — eviction simply drops the world's clone,
//! the txn's clone keeps the chunk alive) and acquires the per-chunk
//! locks in lex-coord order. Reads and writes outside the declared
//! set return [`TxnError::OutOfRange`].
//!
//! This file implements only the synchronous variants of
//! `begin_*_txn`, which return [`TxnError::NotLoaded`] when any
//! working-set coord isn't in memory yet.

use std::collections::HashMap;
use std::sync::Arc;

use dashmap::DashMap;
use parking_lot::{ArcRwLockReadGuard, ArcRwLockWriteGuard, RawRwLock};

use crate::core::blocks::BlockData;
use crate::core::math::{Vec3i, Vec3u};

use super::chunk::{Chunk, ChunkData};
use super::{World, block_coord, chunk_coord};

// ----------------------------------------------------------------------
//   WorkingSet
// ----------------------------------------------------------------------

#[derive(Debug, Clone)]
pub enum WorkingSet {
    Single(Vec3i),
    List(Vec<Vec3i>),
    /// Inclusive lower, exclusive upper bounds in chunk-coord space.
    Range(Vec3i, Vec3i),
}

impl WorkingSet {
    fn collect_sorted(&self) -> Vec<Vec3i> {
        let mut out: Vec<Vec3i> = match self {
            Self::Single(c) => vec![*c],
            Self::List(v) => v.clone(),
            Self::Range(lo, hi) => {
                let mut v = Vec::new();
                for x in lo.x..hi.x {
                    for y in lo.y..hi.y {
                        for z in lo.z..hi.z {
                            v.push(Vec3i::new(x, y, z));
                        }
                    }
                }
                v
            }
        };
        out.sort_by_key(|c| (c.x, c.y, c.z));
        out.dedup();
        out
    }
}

impl From<Vec3i> for WorkingSet {
    fn from(c: Vec3i) -> Self {
        Self::Single(c)
    }
}
impl From<Vec<Vec3i>> for WorkingSet {
    fn from(v: Vec<Vec3i>) -> Self {
        Self::List(v)
    }
}
impl From<&[Vec3i]> for WorkingSet {
    fn from(s: &[Vec3i]) -> Self {
        Self::List(s.to_vec())
    }
}

// ----------------------------------------------------------------------
//   Errors
// ----------------------------------------------------------------------

/// Single error type covering both txn-begin and txn-access failures.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum TxnError {
    /// Begin: one or more working-set coords aren't currently in memory.
    /// Caller should warm the cache (sync prefetch or async load) and
    /// retry.
    #[error("chunks not loaded: {0:?}")]
    NotLoaded(Vec<Vec3i>),
    /// Access: caller asked for a block whose chunk isn't in the txn's
    /// declared working set.
    #[error("coord {coord:?} is out of the txn's working set")]
    OutOfRange { coord: Vec3i },
}

// ----------------------------------------------------------------------
//   ReadTxn
// ----------------------------------------------------------------------

/// One pinned chunk held by a `ReadTxn`. The owned arc-lock `guard`
/// internally clones the chunk's `Arc<RwLock<Blocks>>`, so the
/// blocks allocation outlives the guard regardless of what the world
/// does to the parent `Chunk` — no separate pin field needed.
struct ReadEntry {
    coord: Vec3i,
    guard: ArcRwLockReadGuard<RawRwLock, ChunkData>,
}

pub struct ReadTxn {
    entries: Vec<ReadEntry>,
}

impl ReadTxn {
    fn entry(&self, cc: Vec3i) -> Option<&ReadEntry> {
        self.entries.iter().find(|e| e.coord == cc)
    }

    /// Read one block. Errors `OutOfRange` if the containing chunk
    /// isn't in this txn's working set.
    pub fn read(&self, coord: Vec3i) -> Result<BlockData, TxnError> {
        let cc = chunk_coord(coord);
        let entry = self.entry(cc).ok_or(TxnError::OutOfRange { coord })?;
        Ok(entry.guard.block(block_coord(coord)))
    }

    /// Borrow the held cell array at `ccoord`. Returns `None` if the
    /// coord isn't in this txn's working set. Borrowed for the
    /// lifetime of `&self`.
    pub fn chunk_at(&self, ccoord: Vec3i) -> Option<&ChunkData> {
        self.entry(ccoord).map(|e| &*e.guard)
    }
}

// ----------------------------------------------------------------------
//   WriteTxn
// ----------------------------------------------------------------------

struct WriteEntry {
    coord: Vec3i,
    chunk: Arc<Chunk>,
    guard: ArcRwLockWriteGuard<RawRwLock, ChunkData>,
}

pub struct WriteTxn {
    entries: Vec<WriteEntry>,
    buffered: HashMap<Vec3i, Vec<(Vec3u, BlockData)>>,
}

impl WriteTxn {
    fn entry(&self, cc: Vec3i) -> Option<&WriteEntry> {
        self.entries.iter().find(|e| e.coord == cc)
    }

    fn entry_mut(&mut self, cc: Vec3i) -> Option<&mut WriteEntry> {
        self.entries.iter_mut().find(|e| e.coord == cc)
    }

    /// Read one block. Sees committed state plus this txn's buffered
    /// writes (last-write-wins).
    pub fn read(&self, coord: Vec3i) -> Result<BlockData, TxnError> {
        let cc = chunk_coord(coord);
        let entry = self.entry(cc).ok_or(TxnError::OutOfRange { coord })?;
        let bcoord = block_coord(coord);
        if let Some(buf) = self.buffered.get(&cc)
            && let Some((_, data)) = buf.iter().rev().find(|(b, _)| *b == bcoord)
        {
            return Ok(*data);
        }
        Ok(entry.guard.block(bcoord))
    }

    /// Buffer one write.
    pub fn write(&mut self, coord: Vec3i, data: BlockData) -> Result<(), TxnError> {
        let cc = chunk_coord(coord);
        if self.entry(cc).is_none() {
            return Err(TxnError::OutOfRange { coord });
        }
        self.buffered
            .entry(cc)
            .or_default()
            .push((block_coord(coord), data));
        Ok(())
    }

    /// Apply all buffered writes in place under the held write guards
    /// and bump each touched chunk's commit generation. Bumping under
    /// the held lock makes the chunk "dirty" (`save_gen < commit_gen`).
    pub fn commit(mut self) {
        let drained: Vec<(Vec3i, Vec<(Vec3u, BlockData)>)> = self.buffered.drain().collect();
        for (cc, writes) in drained {
            let Some(entry) = self.entry_mut(cc) else {
                debug_assert!(false, "buffered write for coord without entry");
                continue;
            };
            for (bcoord, data) in writes {
                *entry.guard.block_mut(bcoord) = data;
            }
            entry.chunk.bump_commit_gen();
        }
    }
}

// ----------------------------------------------------------------------
//   Begin transactions (sync)
// ----------------------------------------------------------------------

pub(super) fn begin_read_sync(world: &World, set: WorkingSet) -> Result<ReadTxn, TxnError> {
    begin_read_in(world.chunks(), set)
}

fn begin_read_in(
    chunks: &DashMap<Vec3i, Arc<Chunk>>,
    set: WorkingSet,
) -> Result<ReadTxn, TxnError> {
    let coords = set.collect_sorted();
    let resolved = resolve_chunks_in(chunks, &coords)?;
    let entries: Vec<ReadEntry> = coords
        .into_iter()
        .zip(resolved)
        .map(|(coord, chunk)| {
            let guard = chunk.read_blocks();
            ReadEntry { coord, guard }
        })
        .collect();
    Ok(ReadTxn { entries })
}

pub(super) fn begin_write_sync(world: &World, set: WorkingSet) -> Result<WriteTxn, TxnError> {
    begin_write_in(world.chunks(), set)
}

fn begin_write_in(
    chunks: &DashMap<Vec3i, Arc<Chunk>>,
    set: WorkingSet,
) -> Result<WriteTxn, TxnError> {
    let coords = set.collect_sorted();
    let resolved = resolve_chunks_in(chunks, &coords)?;
    let entries: Vec<WriteEntry> = coords
        .into_iter()
        .zip(resolved)
        .map(|(coord, chunk)| {
            let guard = chunk.write_blocks();
            WriteEntry {
                coord,
                chunk,
                guard,
            }
        })
        .collect();
    Ok(WriteTxn {
        entries,
        buffered: HashMap::new(),
    })
}

fn resolve_chunks_in(
    chunks: &DashMap<Vec3i, Arc<Chunk>>,
    coords: &[Vec3i],
) -> Result<Vec<Arc<Chunk>>, TxnError> {
    let mut found = Vec::with_capacity(coords.len());
    let mut missing = Vec::new();
    for c in coords {
        match chunks.get(c).map(|r| Arc::clone(r.value())) {
            Some(p) => found.push(p),
            None => missing.push(*c),
        }
    }
    if missing.is_empty() {
        Ok(found)
    } else {
        Err(TxnError::NotLoaded(missing))
    }
}

// ----------------------------------------------------------------------
//   Tests
// ----------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::blocks::{BaseBlocks, BlockLight, BlockRegistry, register_base_blocks};

    fn make_base() -> BaseBlocks {
        let mut reg = BlockRegistry::new();
        register_base_blocks(&mut reg)
    }

    fn install_chunks(chunks: &DashMap<Vec3i, Arc<Chunk>>, coords: &[Vec3i]) {
        for c in coords {
            chunks.insert(
                *c,
                Arc::new(Chunk::from_gen(ChunkData::air_filled(BlockLight::SKY))),
            );
        }
    }

    fn chunk_at(chunks: &DashMap<Vec3i, Arc<Chunk>>, cc: Vec3i) -> Arc<Chunk> {
        chunks
            .get(&cc)
            .map(|r| Arc::clone(r.value()))
            .expect("chunk")
    }

    #[test]
    fn working_set_collect_sorted_dedups_and_orders() {
        let s = WorkingSet::List(vec![
            Vec3i::new(2, 0, 0),
            Vec3i::new(0, 0, 0),
            Vec3i::new(0, 0, 0),
            Vec3i::new(1, 0, 0),
        ]);
        assert_eq!(
            s.collect_sorted(),
            vec![
                Vec3i::new(0, 0, 0),
                Vec3i::new(1, 0, 0),
                Vec3i::new(2, 0, 0),
            ]
        );
    }

    #[test]
    fn working_set_aabb_iterates_inclusive_lo_exclusive_hi() {
        let s = WorkingSet::Range(Vec3i::new(0, 0, 0), Vec3i::new(2, 2, 1));
        let coords = s.collect_sorted();
        assert_eq!(coords.len(), 4);
        assert!(coords.contains(&Vec3i::new(0, 0, 0)));
        assert!(!coords.contains(&Vec3i::new(2, 0, 0)));
    }

    #[test]
    fn begin_read_sync_errors_when_coord_not_loaded() {
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        match begin_read_in(&chunks, WorkingSet::Single(Vec3i::new(0, 0, 0))) {
            Err(TxnError::NotLoaded(v)) => assert_eq!(v, vec![Vec3i::new(0, 0, 0)]),
            Err(other) => panic!("expected NotLoaded; got {other:?}"),
            Ok(_) => panic!("expected NotLoaded; got Ok"),
        }
    }

    #[test]
    fn read_txn_returns_air_for_empty_chunk_in_set() {
        let base = make_base();
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 1, 0);
        install_chunks(&chunks, &[cc]);
        let txn = begin_read_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let b = txn.read(Vec3i::new(0, 16, 0)).expect("in range");
        assert_eq!(b.id, base.air);
    }

    #[test]
    fn read_txn_out_of_range_returns_error() {
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 0, 0);
        install_chunks(&chunks, &[cc]);
        let txn = begin_read_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let err = txn.read(Vec3i::new(16, 0, 0)).expect_err("out of range");
        assert_eq!(
            err,
            TxnError::OutOfRange {
                coord: Vec3i::new(16, 0, 0)
            }
        );
    }

    #[test]
    fn write_txn_commit_makes_value_visible_to_subsequent_read_txn() {
        let base = make_base();
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 0, 0);
        install_chunks(&chunks, &[cc]);

        let mut wtxn = begin_write_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let target = BlockData {
            id: base.rock,
            ..BlockData::default()
        };
        wtxn.write(Vec3i::new(3, 4, 5), target).expect("in range");
        wtxn.commit();

        let rtxn = begin_read_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let b = rtxn.read(Vec3i::new(3, 4, 5)).expect("in range");
        assert_eq!(b.id, base.rock);

        let chunk = chunk_at(&chunks, cc);
        assert!(chunk.dirty());
        assert_eq!(chunk.commit_gen(), 2);
        assert_eq!(chunk.save_gen(), 0);
    }

    #[test]
    fn write_txn_drop_without_commit_rolls_back() {
        let base = make_base();
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 0, 0);
        install_chunks(&chunks, &[cc]);
        {
            let mut wtxn = begin_write_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
            let target = BlockData {
                id: base.rock,
                ..BlockData::default()
            };
            wtxn.write(Vec3i::new(3, 4, 5), target).expect("in range");
        }
        let rtxn = begin_read_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let b = rtxn.read(Vec3i::new(3, 4, 5)).expect("in range");
        assert_eq!(b.id, base.air);
        let chunk = chunk_at(&chunks, cc);
        assert_eq!(chunk.commit_gen(), 1);
    }

    #[test]
    fn write_txn_self_read_sees_buffered_writes_before_commit() {
        let base = make_base();
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 0, 0);
        install_chunks(&chunks, &[cc]);
        let mut wtxn = begin_write_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        let target = BlockData {
            id: base.rock,
            ..BlockData::default()
        };
        let coord = Vec3i::new(1, 1, 1);
        wtxn.write(coord, target).expect("in range");
        let b = wtxn.read(coord).expect("in range");
        assert_eq!(b.id, base.rock);
    }

    #[test]
    fn arc_strong_count_tracks_active_txns() {
        let chunks: DashMap<Vec3i, Arc<Chunk>> = DashMap::new();
        let cc = Vec3i::new(0, 0, 0);
        install_chunks(&chunks, &[cc]);
        let chunk = chunk_at(&chunks, cc);
        // Map holds 1 + our local clone = 2
        assert_eq!(Arc::strong_count(&chunk), 2);
        let wtxn = begin_write_in(&chunks, WorkingSet::Single(cc)).expect("loaded");
        // Now: map + local + txn = 3
        assert_eq!(Arc::strong_count(&chunk), 3);
        drop(wtxn);
        assert_eq!(Arc::strong_count(&chunk), 2);
    }
}
