//! Block id newtype.

use serde::{Deserialize, Serialize};

/// A block id. `BlockId::EMPTY` (== `BlockId(0)`) is the registry's reserved
/// empty/null slot, present in every `BlockRegistry` and what
/// `BlockData::default()` references. The base game maps `air` to this same
/// slot so existing call sites that compare `cell.id == base.air` keep
/// matching the implicit empty cell.
#[repr(C)]
#[derive(
    Copy, Clone, Debug, Eq, PartialEq, Hash, Ord, PartialOrd, Default, Serialize, Deserialize,
)]
pub struct BlockId(pub u16);

impl BlockId {
    /// Smallest representable id and the registry's reserved empty slot.
    pub const EMPTY: BlockId = BlockId(0);
    /// Largest representable id.
    pub const MAX: BlockId = BlockId(u16::MAX);

    #[must_use]
    pub const fn new(value: u16) -> Self {
        Self(value)
    }

    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}
