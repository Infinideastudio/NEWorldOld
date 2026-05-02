//! `WorkingSet`, `ReadTxn`, `WriteTxn` — transactional access to the page
//! store.
//!
//! All transactions declare a *working set* (one chunk, an explicit list,
//! or an axis-aligned chunk-coord box). At begin time the txn pins every
//! page in its working set, then acquires the per-page locks in
//! lex-coord order. Reads and writes outside the declared set return
//! [`AccessError::OutOfRange`].
//!
//! This file implements only the synchronous variants of `begin_*_txn`,
//! which return [`BeginError::NotLoaded`] when any working-set coord
//! isn't in memory yet. The async variants (which dispatch loads through
//! the chunk pipeline) arrive in a later PR.

use std::collections::HashMap;
use std::sync::Arc;

use parking_lot::{ArcRwLockReadGuard, ArcRwLockWriteGuard, RawRwLock};

use super::chunk::Chunk;
use super::page::Page;
use super::page_table::PageTable;
use crate::blocks::{BaseBlocks, BlockData};
use crate::math::{Vec3i, Vec3u};

// ----------------------------------------------------------------------
//   WorkingSet
// ----------------------------------------------------------------------

/// Set of chunk coords a transaction may touch. Construct via [`From`]
/// from the natural shapes (single coord, vec, slice, AABB pair).
#[derive(Debug, Clone)]
pub enum WorkingSet {
    /// One chunk coord.
    Single(Vec3i),
    /// Explicit list. Duplicates are removed at txn-begin.
    Pages(Vec<Vec3i>),
    /// Inclusive lower / exclusive upper chunk-coord box.
    Aabb(Vec3i, Vec3i),
}

impl WorkingSet {
    /// Materialize the set as a sorted, deduplicated `Vec<Vec3i>` ready
    /// for lex-order lock acquisition.
    fn collect_sorted(&self) -> Vec<Vec3i> {
        let mut out: Vec<Vec3i> = match self {
            Self::Single(c) => vec![*c],
            Self::Pages(v) => v.clone(),
            Self::Aabb(lo, hi) => {
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
        out.sort_by(|a, b| (a.x, a.y, a.z).cmp(&(b.x, b.y, b.z)));
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
        Self::Pages(v)
    }
}

impl From<&[Vec3i]> for WorkingSet {
    fn from(s: &[Vec3i]) -> Self {
        Self::Pages(s.to_vec())
    }
}

// ----------------------------------------------------------------------
//   Errors
// ----------------------------------------------------------------------

/// Reasons a `begin_*_txn` call may fail.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum BeginError {
    /// Sync variant only: at least one coord in the working set is not
    /// in memory. The caller must arrange a load (via the async variant
    /// or the pipeline prefetch path) and retry.
    NotLoaded(Vec<Vec3i>),
    /// Async variant only (placeholder for the next PR): coord has no
    /// in-memory copy and no on-disk copy. Caller should generate the
    /// chunk and `create_chunk` it.
    ChunkFault(Vec<Vec3i>),
}

/// Reasons a `txn.read` / `txn.write` may fail.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum AccessError {
    /// The block coord's containing chunk is not in this txn's working
    /// set.
    OutOfRange { coord: Vec3i },
}

// ----------------------------------------------------------------------
//   Pin guard
// ----------------------------------------------------------------------

/// RAII pin: increments [`Page::pin`] on construction, decrements on
/// drop. Field order in transaction structs is significant — pins must
/// be declared *after* lock guards so the locks release before the
/// pins drop.
struct PinGuard {
    page: Arc<Page>,
}

impl PinGuard {
    fn new(page: Arc<Page>) -> Self {
        page.pin();
        Self { page }
    }
}

impl Drop for PinGuard {
    fn drop(&mut self) {
        self.page.unpin();
    }
}

// ----------------------------------------------------------------------
//   Coord helpers
// ----------------------------------------------------------------------

fn chunk_coord_of(c: Vec3i) -> Vec3i {
    Vec3i::new(
        c.x >> Chunk::SIZE_LOG,
        c.y >> Chunk::SIZE_LOG,
        c.z >> Chunk::SIZE_LOG,
    )
}

fn block_offset_of(c: Vec3i) -> Vec3u {
    let mask = Chunk::SIZE - 1;
    Vec3u::new(
        (c.x & mask) as u32,
        (c.y & mask) as u32,
        (c.z & mask) as u32,
    )
}

// ----------------------------------------------------------------------
//   ReadTxn
// ----------------------------------------------------------------------

struct ReadEntry {
    coord: Vec3i,
    /// Owns its own `Arc<RwLock<Chunk>>` reference; releases the read
    /// lock on drop. Declared before `pin` so the lock is released
    /// before the pin count is decremented.
    guard: ArcRwLockReadGuard<RawRwLock, Chunk>,
    pin: PinGuard,
}

/// One read transaction over a fixed working set. Holds a read guard
/// on every page in the set until dropped.
pub struct ReadTxn {
    entries: Vec<ReadEntry>,
}

impl ReadTxn {
    fn entry(&self, cc: Vec3i) -> Option<&ReadEntry> {
        self.entries.iter().find(|e| e.coord == cc)
    }

    /// Read one block. Returns `Err(OutOfRange)` if the containing chunk
    /// isn't in this txn's working set.
    pub fn read(&self, coord: Vec3i, base: &BaseBlocks) -> Result<BlockData, AccessError> {
        let cc = chunk_coord_of(coord);
        let entry = self.entry(cc).ok_or(AccessError::OutOfRange { coord })?;
        Ok(entry.guard.block(block_offset_of(coord), base))
    }
}

// ----------------------------------------------------------------------
//   WriteTxn
// ----------------------------------------------------------------------

struct WriteEntry {
    coord: Vec3i,
    /// Owns its own `Arc<RwLock<Chunk>>`; releases the write lock on
    /// drop. Declared before `pin` for the same reason as `ReadEntry`.
    guard: ArcRwLockWriteGuard<RawRwLock, Chunk>,
    pin: PinGuard,
}

/// One write transaction over a fixed working set. Buffers writes
/// in-memory; [`Self::commit`] applies them in place under the held
/// write guards. Dropping without committing rolls back (no buffered
/// state ever touches the chunks).
pub struct WriteTxn {
    entries: Vec<WriteEntry>,
    /// Buffered writes keyed by chunk coord, drained during `commit`.
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
    /// writes (the latter overlay the former, last-write-wins).
    pub fn read(&self, coord: Vec3i, base: &BaseBlocks) -> Result<BlockData, AccessError> {
        let cc = chunk_coord_of(coord);
        let entry = self.entry(cc).ok_or(AccessError::OutOfRange { coord })?;
        let bcoord = block_offset_of(coord);
        if let Some(buf) = self.buffered.get(&cc)
            && let Some((_, data)) = buf.iter().rev().find(|(b, _)| *b == bcoord)
        {
            return Ok(*data);
        }
        Ok(entry.guard.block(bcoord, base))
    }

    /// Buffer one write. The change isn't visible to other transactions
    /// until [`Self::commit`].
    pub fn write(&mut self, coord: Vec3i, data: BlockData) -> Result<(), AccessError> {
        let cc = chunk_coord_of(coord);
        if self.entry(cc).is_none() {
            return Err(AccessError::OutOfRange { coord });
        }
        self.buffered
            .entry(cc)
            .or_default()
            .push((block_offset_of(coord), data));
        Ok(())
    }

    /// Apply all buffered writes in place under the held write guards,
    /// mark each touched page dirty + bump its commit generation, then
    /// release locks and pins via `Drop`. Cannot fail.
    pub fn commit(mut self, base: &BaseBlocks) {
        // Drain buffered into a vec so we can iterate without borrowing
        // self.entries while the map is still live.
        let drained: Vec<(Vec3i, Vec<(Vec3u, BlockData)>)> = self.buffered.drain().collect();
        for (cc, writes) in drained {
            let Some(entry) = self.entry_mut(cc) else {
                // OOR writes were already rejected at write-time; an entry
                // missing here would be a bug.
                debug_assert!(false, "buffered write for coord without entry");
                continue;
            };
            for (bcoord, data) in writes {
                *entry.guard.block_mut(bcoord, base) = data;
            }
            entry.page().mark_dirty();
            entry.page().bump_commit_gen();
        }
        // self drops here; entries (locks) drop before pins, pins decrement.
    }
}

// ----------------------------------------------------------------------
//   Begin transactions (sync)
// ----------------------------------------------------------------------

/// Sync `begin_read_txn`. Errors with `NotLoaded` if any working-set
/// coord isn't currently in `pages`.
pub(super) fn begin_read_sync(
    pages: &PageTable,
    set: WorkingSet,
) -> Result<ReadTxn, BeginError> {
    let coords = set.collect_sorted();
    let resolved = resolve_pages(pages, &coords)?;
    let entries: Vec<ReadEntry> = coords
        .into_iter()
        .zip(resolved)
        .map(|(coord, page)| {
            let pin = PinGuard::new(Arc::clone(&page));
            // read_arc takes &Arc<RwLock<Chunk>> and returns an owned
            // ArcRwLockReadGuard that holds the inner Arc itself.
            let guard = page.chunk_arc().read_arc();
            ReadEntry { coord, guard, pin }
        })
        .collect();
    Ok(ReadTxn { entries })
}

/// Sync `begin_write_txn`. Errors with `NotLoaded` if any working-set
/// coord isn't currently in `pages`.
pub(super) fn begin_write_sync(
    pages: &PageTable,
    set: WorkingSet,
) -> Result<WriteTxn, BeginError> {
    let coords = set.collect_sorted();
    let resolved = resolve_pages(pages, &coords)?;
    let entries: Vec<WriteEntry> = coords
        .into_iter()
        .zip(resolved)
        .map(|(coord, page)| {
            let pin = PinGuard::new(Arc::clone(&page));
            let guard = page.chunk_arc().write_arc();
            WriteEntry { coord, guard, pin }
        })
        .collect();
    Ok(WriteTxn {
        entries,
        buffered: HashMap::new(),
    })
}

/// Look every coord up in `pages`; return all `Arc<Page>`s in the same
/// order as `coords`, or a `NotLoaded` error listing the missing coords.
fn resolve_pages(pages: &PageTable, coords: &[Vec3i]) -> Result<Vec<Arc<Page>>, BeginError> {
    let mut found = Vec::with_capacity(coords.len());
    let mut missing = Vec::new();
    for c in coords {
        match pages.get(*c) {
            Some(p) => found.push(p),
            None => missing.push(*c),
        }
    }
    if missing.is_empty() {
        Ok(found)
    } else {
        Err(BeginError::NotLoaded(missing))
    }
}

// ----------------------------------------------------------------------
//   WriteEntry needs `page` accessible — expose via field
// ----------------------------------------------------------------------

impl WriteEntry {
    /// `mark_dirty` / `bump_commit_gen` go through the pinned `Arc<Page>`
    /// so commits can update paging state without an extra lookup.
    fn page(&self) -> &Arc<Page> {
        &self.pin.page
    }
}

// ----------------------------------------------------------------------
//   Tests
// ----------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{BlockData, BlockRegistry, register_base_blocks};

    fn make_base() -> BaseBlocks {
        let mut reg = BlockRegistry::new();
        register_base_blocks(&mut reg)
    }

    fn install_pages(table: &PageTable, coords: &[Vec3i]) {
        for c in coords {
            table.insert(*c, Arc::new(Page::new(*c, Chunk::new(*c), false)));
        }
    }

    #[test]
    fn working_set_collect_sorted_dedups_and_orders() {
        let s = WorkingSet::Pages(vec![
            Vec3i::new(2, 0, 0),
            Vec3i::new(0, 0, 0),
            Vec3i::new(0, 0, 0), // duplicate
            Vec3i::new(1, 0, 0),
        ]);
        let coords = s.collect_sorted();
        assert_eq!(
            coords,
            vec![
                Vec3i::new(0, 0, 0),
                Vec3i::new(1, 0, 0),
                Vec3i::new(2, 0, 0),
            ]
        );
    }

    #[test]
    fn working_set_aabb_iterates_inclusive_lo_exclusive_hi() {
        let s = WorkingSet::Aabb(Vec3i::new(0, 0, 0), Vec3i::new(2, 2, 1));
        let coords = s.collect_sorted();
        // 2 * 2 * 1 = 4 pages.
        assert_eq!(coords.len(), 4);
        assert!(coords.contains(&Vec3i::new(0, 0, 0)));
        assert!(coords.contains(&Vec3i::new(1, 1, 0)));
        assert!(!coords.contains(&Vec3i::new(2, 0, 0)));
    }

    #[test]
    fn begin_read_sync_errors_when_coord_not_loaded() {
        let table = PageTable::new();
        match begin_read_sync(&table, WorkingSet::Single(Vec3i::new(0, 0, 0))) {
            Err(BeginError::NotLoaded(v)) => assert_eq!(v, vec![Vec3i::new(0, 0, 0)]),
            Err(other) => panic!("expected NotLoaded; got {other:?}"),
            Ok(_) => panic!("expected NotLoaded; got Ok"),
        }
    }

    #[test]
    fn read_txn_returns_air_for_empty_chunk_in_set() {
        let base = make_base();
        let table = PageTable::new();
        let cc = Vec3i::new(0, 1, 0);
        install_pages(&table, &[cc]);
        let txn = begin_read_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        // Block (0, 16, 0) lives in chunk (0, 1, 0) at local (0, 0, 0).
        let b = txn.read(Vec3i::new(0, 16, 0), &base).expect("in range");
        assert_eq!(b.id, base.air);
    }

    #[test]
    fn read_txn_out_of_range_returns_error() {
        let base = make_base();
        let table = PageTable::new();
        let cc = Vec3i::new(0, 0, 0);
        install_pages(&table, &[cc]);
        let txn = begin_read_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        // Block (16, 0, 0) is in chunk (1, 0, 0) — outside the set.
        let err = txn.read(Vec3i::new(16, 0, 0), &base).expect_err("out of range");
        assert_eq!(err, AccessError::OutOfRange { coord: Vec3i::new(16, 0, 0) });
    }

    #[test]
    fn write_txn_commit_makes_value_visible_to_subsequent_read_txn() {
        let base = make_base();
        let table = PageTable::new();
        let cc = Vec3i::new(0, 0, 0);
        install_pages(&table, &[cc]);

        // Commit a rock at (3, 4, 5).
        let mut wtxn = begin_write_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        let target = BlockData {
            id: base.rock,
            ..BlockData::default()
        };
        wtxn.write(Vec3i::new(3, 4, 5), target).expect("in range");
        wtxn.commit(&base);

        // A fresh read txn sees the rock.
        let rtxn = begin_read_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        let b = rtxn.read(Vec3i::new(3, 4, 5), &base).expect("in range");
        assert_eq!(b.id, base.rock);

        // The page is dirty after commit.
        let page = table.get(cc).expect("page");
        assert!(page.dirty());
        assert_eq!(page.commit_gen(), 1);
    }

    #[test]
    fn write_txn_drop_without_commit_rolls_back() {
        let base = make_base();
        let table = PageTable::new();
        let cc = Vec3i::new(0, 0, 0);
        install_pages(&table, &[cc]);

        {
            let mut wtxn =
                begin_write_sync(&table, WorkingSet::Single(cc)).expect("loaded");
            let target = BlockData {
                id: base.rock,
                ..BlockData::default()
            };
            wtxn.write(Vec3i::new(3, 4, 5), target).expect("in range");
            // Drop without commit.
        }

        // The cell is still air.
        let rtxn = begin_read_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        let b = rtxn.read(Vec3i::new(3, 4, 5), &base).expect("in range");
        assert_eq!(b.id, base.air);

        // The page is NOT dirty (commit didn't happen).
        let page = table.get(cc).expect("page");
        assert!(!page.dirty());
        assert_eq!(page.commit_gen(), 0);
    }

    #[test]
    fn write_txn_self_read_sees_buffered_writes_before_commit() {
        let base = make_base();
        let table = PageTable::new();
        let cc = Vec3i::new(0, 0, 0);
        install_pages(&table, &[cc]);

        let mut wtxn = begin_write_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        let target = BlockData {
            id: base.rock,
            ..BlockData::default()
        };
        let coord = Vec3i::new(1, 1, 1);
        wtxn.write(coord, target).expect("in range");
        // Read before commit sees the buffered value.
        let b = wtxn.read(coord, &base).expect("in range");
        assert_eq!(b.id, base.rock);
    }

    #[test]
    fn write_txn_pin_count_is_one_during_txn_zero_after_drop() {
        let table = PageTable::new();
        let cc = Vec3i::new(0, 0, 0);
        install_pages(&table, &[cc]);

        let page = table.get(cc).expect("page");
        assert_eq!(page.pin_count(), 0);

        let wtxn = begin_write_sync(&table, WorkingSet::Single(cc)).expect("loaded");
        assert_eq!(page.pin_count(), 1);
        drop(wtxn);
        assert_eq!(page.pin_count(), 0);
    }
}

