//! `Page` — one slot in the [`PageTable`](super::page_table::PageTable).
//!
//! A page wraps a single [`Chunk`] under a `parking_lot::RwLock` and carries
//! the lock-free atomics the world's eviction / writeback / 2PL machinery
//! needs to consult without taking the chunk lock:
//!
//! - `pin`     — non-zero ⇒ eviction must skip this page.
//! - `dirty`   — committed-but-not-yet-written-back.
//! - `commit_gen` — bumped on every commit; lets writeback detect concurrent
//!   commits and avoid clearing `dirty` for changes it didn't capture.
//! - `on_disk` — true after a successful load OR a successful writeback.
//!
//! The lock and the atomics are deliberately split. Embedding the atomics
//! into [`Chunk`] would force every `chunk.block()` / `chunk.package_to()`
//! through a lock guard for what is conceptually paging metadata, not
//! block data. Keeping `Page` as the cache-slot wrapper preserves the
//! existing `Chunk` API.

use std::sync::Arc;
use std::sync::atomic::{AtomicBool, AtomicU32, AtomicU64, Ordering};

use parking_lot::RwLock;

use super::chunk::Chunk;
use crate::math::Vec3i;

/// One cache slot in the world's page store.
///
/// The chunk lock is wrapped in its own `Arc` so transactions can hold
/// `parking_lot::ArcRwLock{Read,Write}Guard` — owned guards that keep the
/// lock alive without borrowing from `Page`. That lets a `WriteTxn`
/// store `(Arc<Page>, ArcRwLockWriteGuard<Chunk>)` together as one
/// non-self-referential value. The atomics stay on `Page` (outside the
/// inner Arc) so eviction / writeback / pin sweeps don't have to lock
/// the chunk to read paging metadata.
pub(crate) struct Page {
    coord: Vec3i,
    chunk: Arc<RwLock<Chunk>>,
    pin: AtomicU32,
    dirty: AtomicBool,
    commit_gen: AtomicU64,
    on_disk: AtomicBool,
}

impl Page {
    /// New page wrapping `chunk` at `coord`. `on_disk` is set per the
    /// caller — true if loaded from sled, false if freshly generated.
    #[must_use]
    pub(crate) fn new(coord: Vec3i, chunk: Chunk, on_disk: bool) -> Self {
        Self {
            coord,
            chunk: Arc::new(RwLock::new(chunk)),
            pin: AtomicU32::new(0),
            dirty: AtomicBool::new(false),
            commit_gen: AtomicU64::new(0),
            on_disk: AtomicBool::new(on_disk),
        }
    }

    /// Chunk coordinate this page sits at.
    #[must_use]
    pub(crate) fn coord(&self) -> Vec3i {
        self.coord
    }

    /// Borrow the inner `Arc<RwLock<Chunk>>` so callers can take owned
    /// `ArcRwLock{Read,Write}Guard`s via `read_arc()` / `write_arc()`.
    #[must_use]
    pub(crate) fn chunk_arc(&self) -> &Arc<RwLock<Chunk>> {
        &self.chunk
    }

    // ----- pin -----

    /// Increment the pin count. While `pin > 0` the page is exempt from
    /// eviction.
    pub(crate) fn pin(&self) {
        self.pin.fetch_add(1, Ordering::AcqRel);
    }

    /// Decrement the pin count. The caller must have previously called
    /// [`Self::pin`]; underflow is a bug.
    pub(crate) fn unpin(&self) {
        let prev = self.pin.fetch_sub(1, Ordering::AcqRel);
        debug_assert!(prev > 0, "Page::unpin underflow at {:?}", self.coord);
    }

    /// Current pin count. Used by eviction to skip pinned pages.
    #[must_use]
    pub(crate) fn pin_count(&self) -> u32 {
        self.pin.load(Ordering::Acquire)
    }

    // ----- dirty / commit_gen -----

    /// Mark this page as having uncommitted on-disk state. Set by
    /// [`super::txn::WriteTxn::commit`].
    pub(crate) fn mark_dirty(&self) {
        self.dirty.store(true, Ordering::Release);
    }

    /// True iff there are committed changes not yet written back.
    #[must_use]
    pub(crate) fn dirty(&self) -> bool {
        self.dirty.load(Ordering::Acquire)
    }

    /// Clear the dirty flag iff `expected_gen` still matches the current
    /// commit generation — i.e. no commit landed between the writeback
    /// snapshot and the post-fsync clear. Returns whether the clear
    /// happened.
    pub(crate) fn try_clear_dirty(&self, expected_gen: u64) -> bool {
        if self.commit_gen.load(Ordering::Acquire) == expected_gen {
            self.dirty.store(false, Ordering::Release);
            true
        } else {
            false
        }
    }

    /// Bump the commit generation. Called by [`super::txn::WriteTxn::commit`]
    /// after applying writes under the held write guard.
    pub(crate) fn bump_commit_gen(&self) {
        self.commit_gen.fetch_add(1, Ordering::AcqRel);
    }

    /// Snapshot the current commit generation. Used by writeback to
    /// detect concurrent commits.
    #[must_use]
    pub(crate) fn commit_gen(&self) -> u64 {
        self.commit_gen.load(Ordering::Acquire)
    }

    // ----- on_disk -----

    /// Mark this page as backed by a successful disk write (or load).
    pub(crate) fn mark_on_disk(&self) {
        self.on_disk.store(true, Ordering::Release);
    }

    /// True iff a copy of this page lives on disk. False for
    /// freshly-generated chunks that have never been written back.
    #[must_use]
    pub(crate) fn on_disk(&self) -> bool {
        self.on_disk.load(Ordering::Acquire)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn page_at(coord: Vec3i) -> Page {
        Page::new(coord, Chunk::new(coord), false)
    }

    #[test]
    fn pin_unpin_round_trip() {
        let p = page_at(Vec3i::new(0, 0, 0));
        assert_eq!(p.pin_count(), 0);
        p.pin();
        p.pin();
        assert_eq!(p.pin_count(), 2);
        p.unpin();
        assert_eq!(p.pin_count(), 1);
        p.unpin();
        assert_eq!(p.pin_count(), 0);
    }

    #[test]
    fn dirty_default_false_set_and_clear() {
        let p = page_at(Vec3i::new(1, 2, 3));
        assert!(!p.dirty());
        p.mark_dirty();
        p.bump_commit_gen();
        assert!(p.dirty());
        // Clear succeeds when the captured gen still matches.
        assert!(p.try_clear_dirty(p.commit_gen()));
        assert!(!p.dirty());
    }

    #[test]
    fn try_clear_dirty_skips_when_commit_landed() {
        let p = page_at(Vec3i::new(0, 0, 0));
        p.mark_dirty();
        p.bump_commit_gen();
        let snapshot_gen = p.commit_gen();
        // A second commit lands before writeback finishes its fsync.
        p.bump_commit_gen();
        // try_clear_dirty must NOT clear — the captured snapshot is stale.
        assert!(!p.try_clear_dirty(snapshot_gen));
        assert!(p.dirty());
        // Re-snapshotting picks up the new gen.
        assert!(p.try_clear_dirty(p.commit_gen()));
        assert!(!p.dirty());
    }

    #[test]
    fn on_disk_default_per_constructor() {
        let p_fresh = Page::new(Vec3i::new(0, 0, 0), Chunk::new(Vec3i::new(0, 0, 0)), false);
        assert!(!p_fresh.on_disk());
        p_fresh.mark_on_disk();
        assert!(p_fresh.on_disk());

        let p_loaded = Page::new(Vec3i::new(0, 0, 0), Chunk::new(Vec3i::new(0, 0, 0)), true);
        assert!(p_loaded.on_disk());
    }
}
