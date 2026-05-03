//! Owning registry of `BlockInfo` entries.
//!
//! Constructed once at boot (typically via `register_base_blocks`) then
//! wrapped in `Arc<BlockRegistry>` for shared read-only access. No
//! module-level globals — the registry is owned by the caller.
//!
//! `BlockId(0)` is reserved as the empty/null slot, pre-installed with
//! `BlockInfo::empty()` by every freshly constructed registry. Out-of-range
//! id lookups return that same entry, so `BlockData::default()` (id 0) and
//! garbage ids both render as "nothing here". Mods that want to override the
//! empty slot's appearance call [`BlockRegistry::set`].

use std::borrow::Cow;
use std::collections::HashMap;

use super::{BlockId, BlockInfo};

/// Owning registry of `BlockInfo` entries.
#[derive(Clone, Debug)]
pub struct BlockRegistry {
    by_id: Vec<BlockInfo>,
    by_name: HashMap<Cow<'static, str>, BlockId>,
}

impl BlockRegistry {
    /// Empty registry with `BlockInfo::default()` mapped to the empty block.
    pub fn new() -> Self {
        let mut r = Self {
            by_id: Vec::new(),
            by_name: HashMap::new(),
        };
        r.add(BlockInfo::new("empty", "Empty"));
        r
    }

    /// Register `info` and return its assigned id. If `info.name` is already
    /// registered the existing slot is overwritten in place — the caller
    /// gets back the same id, no shifting of later entries. Used by mods to
    /// patch an engine block without disturbing ids below it.
    ///
    /// Panics if the id space (`u16`) is exhausted.
    pub fn add(&mut self, info: BlockInfo) -> BlockId {
        if let Some(&id) = self.by_name.get(&info.name) {
            self.by_id[id.get() as usize] = info;
            return id;
        }
        let id = BlockId::new(u16::try_from(self.by_id.len()).expect("block ID space exhausted"));
        self.by_name.insert(info.name.clone(), id);
        self.by_id.push(info);
        id
    }

    /// Look up block info by id. Out-of-range ids return the empty fallback
    /// (`BlockId::EMPTY`'s entry).
    pub fn get(&self, id: BlockId) -> &BlockInfo {
        self.by_id.get(id.get() as usize).unwrap_or(&self.by_id[0])
    }

    /// Strict variant: returns `None` when the id is unknown.
    pub fn try_get(&self, id: BlockId) -> Option<&BlockInfo> {
        self.by_id.get(id.get() as usize)
    }

    /// Resolve a block name to its id.
    pub fn id_of(&self, name: &str) -> Option<BlockId> {
        self.by_name.get(name).copied()
    }

    /// All registered entries in id order.
    pub fn entries(&self) -> &[BlockInfo] {
        &self.by_id
    }

    /// Number of registered blocks (including the reserved empty slot).
    pub fn len(&self) -> usize {
        self.by_id.len()
    }

    /// True iff only the implicit empty entry has been registered.
    pub fn is_empty(&self) -> bool {
        self.by_id.len() <= 1
    }
}

impl Default for BlockRegistry {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rock(name: &'static str) -> BlockInfo {
        BlockInfo::new(name, name)
            .solid(true)
            .opaque(true)
            .hardness(1.0)
    }

    #[test]
    fn empty_entry_lives_at_id_zero() {
        let r = BlockRegistry::new();
        assert_eq!(r.len(), 1);
        let empty = r.get(BlockId::default());
        assert_eq!(empty.name, "empty");
        assert!(!empty.solid);
        // Out-of-range falls back to id 0.
        assert_eq!(r.get(BlockId::new(42)), empty);
    }

    #[test]
    fn add_assigns_sequential_ids_and_indexes_by_name() {
        let mut r = BlockRegistry::new();
        let a = r.add(rock("a"));
        let b = r.add(rock("b"));
        assert_eq!(a, BlockId::new(1));
        assert_eq!(b, BlockId::new(2));
        assert_eq!(r.id_of("a"), Some(BlockId::new(1)));
        assert_eq!(r.id_of("missing"), None);
    }

    #[test]
    fn re_registering_a_name_overwrites_in_place() {
        let mut r = BlockRegistry::new();
        let id = r.add(rock("rock"));
        let mut patched = rock("rock");
        patched.hardness = 99.0;
        let id2 = r.add(patched);
        assert_eq!(id, id2);
        assert_eq!(r.get(id).hardness, 99.0);
        assert_eq!(r.len(), 2);
    }
}
