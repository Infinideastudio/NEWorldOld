//! Inventory item stacks.
//!
//! Port of the C++ `items.ixx::ItemStack`. The C++ original uses `size_t` for
//! `count`, but the inventory math in `src/neworld.ixx::draw_inventory` caps at
//! 255 — so `u8` is the right type here (per `docs/rust_migration.md` §4.17).
//! Because the player inventory is saved as a contiguous block, `ItemStack` is
//! `#[repr(C)]` and `bytemuck::Pod`.

use bytemuck::{Pod, Zeroable};
use serde::{Deserialize, Serialize};

use crate::blocks;

/// A stack of `count` items of block id `id`. `Default` is the empty stack
/// (`Id::default()`, count 0), matching the C++ `ItemStack()` default.
///
/// Layout note: `#[repr(C)]` with an `Id` (`u16`) and a `u8` would normally
/// leave a trailing pad byte, which `bytemuck::Pod` forbids. The explicit
/// `_pad` field makes the padding addressable so `Pod` can be derived; serde
/// skips it so the field never appears in serialized output.
#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Default, Pod, Zeroable, Serialize, Deserialize)]
pub struct ItemStack {
    pub id: blocks::Id,
    pub count: u8,
    #[serde(skip, default)]
    _pad: u8,
}

impl ItemStack {
    /// Maximum number of items in a single stack. Matches the `<= 255` and
    /// `255 - slot.count` arithmetic in the C++ `draw_inventory` UI.
    pub const MAX_COUNT: u8 = 255;

    /// Construct a stack of `count` items of the given block id.
    #[must_use]
    pub const fn new(id: blocks::Id, count: u8) -> Self {
        Self { id, count, _pad: 0 }
    }

    /// True iff `count == 0` (mirrors C++ `bool empty() const noexcept`).
    #[must_use]
    pub const fn empty(&self) -> bool {
        self.count == 0
    }

    /// True iff `count == MAX_COUNT`.
    #[must_use]
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

// Compile-time confirmation that `ItemStack` is `Pod` — the player save
// layer relies on this for the inventory blob.
const _: () = {
    const fn assert_pod<T: bytemuck::Pod>() {}
    assert_pod::<ItemStack>();
};

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
        let s = ItemStack::new(blocks::Id(7), ItemStack::MAX_COUNT);
        assert!(s.is_full());
        let almost = ItemStack::new(blocks::Id(7), ItemStack::MAX_COUNT - 1);
        assert!(!almost.is_full());
    }

    #[test]
    fn merge_same_id_under_cap_drains_source() {
        let mut dst = ItemStack::new(blocks::Id(3), 10);
        let mut src = ItemStack::new(blocks::Id(3), 20);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, 30);
        assert_eq!(src.count, 0);
        assert!(src.empty());
    }

    #[test]
    fn merge_same_id_overflow_fills_dst_and_leaves_remainder() {
        let mut dst = ItemStack::new(blocks::Id(3), 200);
        let mut src = ItemStack::new(blocks::Id(3), 100);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, ItemStack::MAX_COUNT);
        assert_eq!(src.count, 45); // 200 + 100 - 255
        assert!(dst.is_full());
    }

    #[test]
    fn merge_different_ids_is_a_noop() {
        let mut dst = ItemStack::new(blocks::Id(3), 10);
        let mut src = ItemStack::new(blocks::Id(4), 20);
        dst.merge_into(&mut src);
        assert_eq!(dst.count, 10);
        assert_eq!(src.count, 20);
    }
}
