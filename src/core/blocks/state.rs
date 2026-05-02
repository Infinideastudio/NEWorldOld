//! 16-bit block state with an external-storage tag bit.
//!
//! Layout: the high bit (`0x8000`) is the *external* flag; the low 15 bits
//! are the payload.
//!
//! * `BlockState(0x0000..=0x7FFF)` — **inline**: the 15-bit value is the
//!   block's full state (orientation, sub-variant, etc.). `BlockState(0)`
//!   is the default and the natural "no-state" value.
//! * `BlockState(0x8000..=0xFFFF)` — **external**: the 15 low bits index
//!   into per-cell state stored outside the chunk grid (signs, chests,
//!   anything bigger than 15 bits). The chunk array still holds a fixed
//!   `BlockState`; whatever side table the world keeps does the lookup
//!   when the block actually needs the larger payload.

use serde::{Deserialize, Serialize};

/// High bit marking external storage.
const EXTERNAL_BIT: u16 = 0x8000;
/// Low 15 bits — payload mask for both variants.
const VALUE_MASK: u16 = 0x7FFF;

/// 16-bit block state. See module doc for layout.
#[repr(C)]
#[derive(
    Copy, Clone, Debug, Eq, PartialEq, Hash, Ord, PartialOrd, Default, Serialize, Deserialize,
)]
pub struct BlockState(pub u16);

impl BlockState {
    /// Maximum 15-bit payload (inline value or external index).
    pub const MAX_PAYLOAD: u16 = VALUE_MASK;

    /// Inline state. `value` must fit in 15 bits.
    #[must_use]
    pub const fn inline(value: u16) -> Self {
        assert!(value <= VALUE_MASK, "BlockState::inline: value > 0x7FFF");
        Self(value)
    }

    /// External-storage tag with the given 15-bit index.
    #[must_use]
    pub const fn external(index: u16) -> Self {
        assert!(index <= VALUE_MASK, "BlockState::external: index > 0x7FFF");
        Self(EXTERNAL_BIT | index)
    }

    /// True iff the high bit is set (external storage).
    #[must_use]
    pub const fn is_external(self) -> bool {
        self.0 & EXTERNAL_BIT != 0
    }

    /// 15-bit external index when [`Self::is_external`], else `None`.
    #[must_use]
    pub const fn external_index(self) -> Option<u16> {
        if self.is_external() {
            Some(self.0 & VALUE_MASK)
        } else {
            None
        }
    }

    /// 15-bit inline value when *not* external, else `None`. The inline
    /// payload is what `face_for` / `Orientation::for_block` consume.
    #[must_use]
    pub const fn inline_value(self) -> Option<u16> {
        if self.is_external() {
            None
        } else {
            Some(self.0 & VALUE_MASK)
        }
    }

    /// Convenience: inline value or zero when external. The chunk mesher and
    /// orientation table both treat external states as "no inline payload",
    /// which falls back to the canonical placement.
    #[must_use]
    pub const fn inline_or_zero(self) -> u16 {
        if self.is_external() {
            0
        } else {
            self.0 & VALUE_MASK
        }
    }

    /// Build from an `Option<u16>`: `Some(v)` → inline (panics if
    /// `v > 0x7FFF`), `None` → external with index `0`.
    #[must_use]
    pub const fn new(value: Option<u16>) -> Self {
        match value {
            None => Self(EXTERNAL_BIT),
            Some(v) => Self::inline(v),
        }
    }

    /// Inverse of [`Self::new`]: `Some(inline_value)` or `None` when
    /// external.
    #[must_use]
    pub const fn get(self) -> Option<u16> {
        self.inline_value()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inline_round_trips() {
        let s = BlockState::inline(7);
        assert!(!s.is_external());
        assert_eq!(s.inline_value(), Some(7));
        assert_eq!(s.inline_or_zero(), 7);
        assert_eq!(s.external_index(), None);
        assert_eq!(BlockState::default(), BlockState(0));
    }

    #[test]
    fn external_round_trips() {
        let s = BlockState::external(42);
        assert!(s.is_external());
        assert_eq!(s.external_index(), Some(42));
        assert_eq!(s.inline_value(), None);
        assert_eq!(s.inline_or_zero(), 0);
    }

    #[test]
    fn high_bit_separates_inline_and_external() {
        let inline_max = BlockState::inline(0x7FFF);
        assert!(!inline_max.is_external());
        assert_eq!(inline_max.0, 0x7FFF);
        let ext_zero = BlockState::external(0);
        assert!(ext_zero.is_external());
        assert_eq!(ext_zero.0, 0x8000);
        let ext_max = BlockState::external(0x7FFF);
        assert_eq!(ext_max.0, 0xFFFF);
    }

    #[test]
    fn legacy_optional_constructor() {
        assert_eq!(BlockState::new(None), BlockState::external(0));
        assert_eq!(BlockState::new(Some(5)), BlockState::inline(5));
        assert_eq!(BlockState::new(Some(5)).get(), Some(5));
        assert!(BlockState::new(None).get().is_none());
    }
}
