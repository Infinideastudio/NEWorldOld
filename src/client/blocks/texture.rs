//! Block-face texture index + the registry that resolves names to indices.
//!
//! Lives in `client/` because a headless server has no concept of textures.
//! The registry is populated alongside `BlockRenderRegistry` from a
//! client-side base-game registration call; the renderer iterates
//! `names()` in registration order to build its GPU strip atlas.

use std::borrow::Cow;
use std::collections::HashMap;

use serde::{Deserialize, Serialize};

/// Index into the block texture atlas. The numeric value is a layer index
/// in the GPU `D2Array` atlas — assigned in registration order by
/// [`BlockTextureRegistry`].
#[repr(C)]
#[derive(
    Copy, Clone, Debug, Eq, PartialEq, Hash, Ord, PartialOrd, Default, Serialize, Deserialize,
)]
pub struct BlockTextureIndex(pub u16);

impl BlockTextureIndex {
    #[must_use]
    pub const fn new(value: u16) -> Self {
        Self(value)
    }

    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

/// Owning registry of block-face texture names. Append-only: registering
/// a name returns a [`BlockTextureIndex`] whose value is the current
/// `len()` and stays valid for the registry's lifetime. Idempotent — a
/// repeat registration of the same name returns the prior index.
///
/// Names are arbitrary strings. The renderer iterates [`Self::names`] in
/// index order to build the GPU strip atlas, loading PNGs from
/// `assets/textures/blocks/diffuse/<name>.png` and similarly `normal/`.
#[derive(Clone, Debug, Default)]
pub struct BlockTextureRegistry {
    names: Vec<Cow<'static, str>>,
    by_name: HashMap<Cow<'static, str>, BlockTextureIndex>,
}

impl BlockTextureRegistry {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn register(&mut self, name: impl Into<Cow<'static, str>>) -> BlockTextureIndex {
        let name = name.into();
        if let Some(&idx) = self.by_name.get(&name) {
            return idx;
        }
        let idx = BlockTextureIndex(
            u16::try_from(self.names.len()).expect("BlockTextureRegistry: index space exhausted"),
        );
        self.names.push(name.clone());
        self.by_name.insert(name, idx);
        idx
    }

    #[must_use]
    pub fn get(&self, name: &str) -> Option<BlockTextureIndex> {
        self.by_name.get(name).copied()
    }

    #[must_use]
    pub fn get_or(&self, name: &str, default: BlockTextureIndex) -> BlockTextureIndex {
        self.get(name).unwrap_or(default)
    }

    #[must_use]
    pub fn name(&self, index: BlockTextureIndex) -> Option<&str> {
        self.names.get(index.0 as usize).map(Cow::as_ref)
    }

    #[must_use]
    pub fn names(&self) -> &[Cow<'static, str>] {
        &self.names
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.names.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.names.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn registers_sequentially_and_is_idempotent() {
        let mut r = BlockTextureRegistry::new();
        let a = r.register("rock");
        let b = r.register("dirt");
        let a2 = r.register("rock");
        assert_eq!(a, BlockTextureIndex(0));
        assert_eq!(b, BlockTextureIndex(1));
        assert_eq!(a, a2);
        assert_eq!(r.len(), 2);
        assert_eq!(r.get("rock"), Some(BlockTextureIndex(0)));
        assert_eq!(r.get("missing"), None);
        assert_eq!(r.name(BlockTextureIndex(1)), Some("dirt"));
    }
}
