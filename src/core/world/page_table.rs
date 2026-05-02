//! `PageTable` — sharded `Vec3i → Arc<Page>` map backing the world's
//! page store.
//!
//! Lookups clone the `Arc<Page>` out and drop the DashMap shard guard
//! immediately. Subsequent operations (lock acquisition, await points,
//! atomic mutations on the `Page`) run against the owned `Arc` without
//! holding any shard lock — that's the only reason the map's value type
//! is `Arc<Page>` rather than bare `Page`.

use std::sync::Arc;

use dashmap::DashMap;

use super::page::Page;
use crate::math::Vec3i;

#[derive(Default)]
pub(crate) struct PageTable {
    map: DashMap<Vec3i, Arc<Page>>,
}

impl PageTable {
    /// Empty table.
    #[must_use]
    pub(crate) fn new() -> Self {
        Self::default()
    }

    /// Look up the page at `coord`, cloning its `Arc` so the caller can
    /// drop the shard guard immediately. `None` if the coord isn't in
    /// the table.
    #[must_use]
    pub(crate) fn get(&self, coord: Vec3i) -> Option<Arc<Page>> {
        self.map.get(&coord).map(|r| Arc::clone(r.value()))
    }

    /// True iff there's a page at `coord`. The shard guard is dropped
    /// before this returns.
    #[must_use]
    pub(crate) fn contains(&self, coord: Vec3i) -> bool {
        self.map.contains_key(&coord)
    }

    /// Insert a fresh page. Overwrites any existing entry — callers that
    /// need create-only semantics should check [`Self::contains`] first
    /// (with the usual TOCTOU caveat) or use [`Self::insert_if_absent`].
    pub(crate) fn insert(&self, coord: Vec3i, page: Arc<Page>) {
        self.map.insert(coord, page);
    }

    /// Insert `page` only if no entry exists at `coord`. Returns
    /// `Ok(())` on insertion, `Err(existing)` if a page was already
    /// present (the input page is returned for the caller to drop or
    /// retry against).
    pub(crate) fn insert_if_absent(
        &self,
        coord: Vec3i,
        page: Arc<Page>,
    ) -> Result<(), Arc<Page>> {
        match self.map.entry(coord) {
            dashmap::mapref::entry::Entry::Occupied(_) => Err(page),
            dashmap::mapref::entry::Entry::Vacant(slot) => {
                slot.insert(page);
                Ok(())
            }
        }
    }

    /// Remove and return the page at `coord`, if any. The caller is
    /// responsible for upholding the eviction invariants
    /// (`pin_count == 0 && !dirty`); this method does no checking.
    #[must_use = "removed page must be dropped or re-inserted"]
    pub(crate) fn remove(&self, coord: Vec3i) -> Option<Arc<Page>> {
        self.map.remove(&coord).map(|(_, v)| v)
    }

    /// Number of pages currently in the table.
    #[must_use]
    pub(crate) fn len(&self) -> usize {
        self.map.len()
    }

    /// True iff the table holds no pages.
    #[must_use]
    pub(crate) fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    /// Iterate every `(coord, Arc<Page>)`. The DashMap iterator holds
    /// shard guards for the duration of iteration; callers that need to
    /// take long actions per page should clone the Arc, collect, then
    /// drop the iterator before working.
    pub(crate) fn iter(
        &self,
    ) -> impl Iterator<Item = dashmap::mapref::multiple::RefMulti<'_, Vec3i, Arc<Page>>> {
        self.map.iter()
    }

    /// Snapshot every `(coord, Arc<Page>)` into an owned `Vec`, dropping
    /// shard guards before returning. Used by sweep passes (eviction,
    /// writeback) that want to iterate without keeping any DashMap
    /// state alive.
    #[must_use]
    pub(crate) fn snapshot(&self) -> Vec<(Vec3i, Arc<Page>)> {
        self.map
            .iter()
            .map(|r| (*r.key(), Arc::clone(r.value())))
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use super::super::chunk::Chunk;

    fn make_page(coord: Vec3i) -> Arc<Page> {
        Arc::new(Page::new(coord, Chunk::new(coord), false))
    }

    #[test]
    fn get_returns_none_for_missing() {
        let t = PageTable::new();
        assert!(t.get(Vec3i::new(0, 0, 0)).is_none());
        assert!(t.is_empty());
    }

    #[test]
    fn insert_then_get_returns_same_arc() {
        let t = PageTable::new();
        let c = Vec3i::new(1, 2, 3);
        let p = make_page(c);
        t.insert(c, Arc::clone(&p));
        let got = t.get(c).expect("present after insert");
        assert!(Arc::ptr_eq(&p, &got));
        assert_eq!(t.len(), 1);
    }

    #[test]
    fn insert_if_absent_rejects_duplicate() {
        let t = PageTable::new();
        let c = Vec3i::new(0, 0, 0);
        let first = make_page(c);
        let second = make_page(c);
        assert!(t.insert_if_absent(c, Arc::clone(&first)).is_ok());
        match t.insert_if_absent(c, Arc::clone(&second)) {
            Err(returned) => assert!(Arc::ptr_eq(&second, &returned)),
            Ok(()) => panic!("second insert must reject"),
        }
        let stored = t.get(c).expect("first page still present");
        assert!(Arc::ptr_eq(&first, &stored));
    }

    #[test]
    fn remove_returns_inserted_page() {
        let t = PageTable::new();
        let c = Vec3i::new(7, 7, 7);
        let p = make_page(c);
        t.insert(c, Arc::clone(&p));
        let removed = t.remove(c).expect("remove returns page");
        assert!(Arc::ptr_eq(&p, &removed));
        assert!(t.is_empty());
        assert!(t.remove(c).is_none(), "remove on empty returns None");
    }

    #[test]
    fn snapshot_is_independent_of_table() {
        let t = PageTable::new();
        for x in 0..3 {
            let c = Vec3i::new(x, 0, 0);
            t.insert(c, make_page(c));
        }
        let snap = t.snapshot();
        assert_eq!(snap.len(), 3);
        // Removing from the table doesn't affect the snapshot.
        t.remove(Vec3i::new(0, 0, 0));
        assert_eq!(snap.len(), 3);
        assert_eq!(t.len(), 2);
    }
}
