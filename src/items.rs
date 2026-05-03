//! Inventory item stacks.
//!
//! Port of the C++ `items.ixx::ItemStack`. The C++ original uses `size_t` for
//! `count`, but the inventory math in `src/neworld.ixx::draw_inventory` caps at
//! 255 — so `u8` is the right type here (per `docs/rust_migration.md` §4.17).
//! The player inventory is saved through bincode (see
//! `worlds::player::save`), so `ItemStack` only needs `Serialize`/`Deserialize`.

use serde::{Deserialize, Serialize};

use crate::core::blocks::BlockId;

/// A stack of `count` items of block id `id`. `Default` is the empty stack
/// (`Id::default()`, count 0), matching the C++ `ItemStack()` default.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Default, Serialize, Deserialize)]
pub struct ItemStack {
    pub id: BlockId,
    pub count: u8,
}

impl ItemStack {
    /// Maximum number of items in a single stack. Matches the `<= 255` and
    /// `255 - slot.count` arithmetic in the C++ `draw_inventory` UI.
    pub const MAX_COUNT: u8 = 255;

    /// Construct a stack of `count` items of the given block id.
    pub const fn new(id: BlockId, count: u8) -> Self {
        Self { id, count }
    }

    /// True iff `count == 0` (mirrors C++ `bool empty() const noexcept`).
    pub const fn empty(&self) -> bool {
        self.count == 0
    }

    /// True iff `count == MAX_COUNT`.
    pub const fn is_full(&self) -> bool {
        self.count == Self::MAX_COUNT
    }

    /// Transfer items from `other` into `self`, capped at [`Self::MAX_COUNT`].
    /// `other.count` is set to whatever did not fit. If the ids differ, both
    /// stacks are left untouched. Captures the
    /// `if (item.count + itemSelected.count <= 255)` arithmetic at
    /// `src/neworld.ixx:1252-1262` in one helper.
    pub fn merge_into(&mut self, other: &mut ItemStack) {
        if self.id != other.id {
            return;
        }
        let space = Self::MAX_COUNT - self.count;
        let moved = space.min(other.count);
        self.count += moved;
        other.count -= moved;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_stack_is_empty() {
        let s = ItemStack::default();
        assert!(s.empty());
        assert_eq!(s.count, 0);
    }

    #[test]
    fn is_full_at_max_count() {
        let s = ItemStack::new(BlockId::new(7), ItemStack::MAX_COUNT);
        assert!(s.is_full());
        let almost = ItemStack::new(BlockId::new(7), ItemStack::MAX_COUNT - 1);
        assert!(!almost.is_full());
    }

    #[test]
    fn merge_same_id_under_cap_drains_source() {
        let mut dst = ItemStack::new(BlockId::new(3), 10);
        let mut src = ItemStack::new(BlockId::new(3), 20);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, 30);
        assert_eq!(src.count, 0);
        assert!(src.empty());
    }

    #[test]
    fn merge_same_id_overflow_fills_dst_and_leaves_remainder() {
        let mut dst = ItemStack::new(BlockId::new(3), 200);
        let mut src = ItemStack::new(BlockId::new(3), 100);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, ItemStack::MAX_COUNT);
        assert_eq!(src.count, 45); // 200 + 100 - 255
        assert!(dst.is_full());
    }

    #[test]
    fn merge_different_ids_is_a_noop() {
        let mut dst = ItemStack::new(BlockId::new(3), 10);
        let mut src = ItemStack::new(BlockId::new(4), 20);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, 10);
        assert_eq!(src.count, 20);
    }
}
