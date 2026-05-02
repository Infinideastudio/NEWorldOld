//! Static, registry-owned properties of a block id.
//!
//! `core::BlockInfo` carries only fields a headless server needs:
//! `name`, `display_name`, `solid`, `opaque`, `translucent`, `hardness`,
//! and `face_mapping`. Texture indices and per-face art live on the
//! client-side `BlockRenderInfo` (see `client::blocks::info`), keyed by
//! the same `BlockId` — registered in a separate pass so a server build
//! never touches the texture registry.
//!
//! Built via the chainable [`BlockInfo::new`] constructor:
//!
//! ```ignore
//! BlockInfo::new("neworld.rock", "Rock")
//!     .solid(true)
//!     .opaque(true)
//!     .hardness(2.0)
//! ```
//!
//! All boolean flags default to `false`, hardness to `0.0`, and
//! `face_mapping` to [`BlockFaceMapping::Static`].

use std::borrow::Cow;

use super::BlockFaceMapping;

/// Static, registry-owned properties of a block id. Server-safe — no
/// rendering / texture concerns.
///
/// Two name fields:
/// * [`Self::name`] — stable internal id, used for save data and registry
///   lookup. Convention is `"<namespace>.<id>"` (e.g. `"neworld.dirt"`).
/// * [`Self::display_name`] — UI label, free-form ("Dirt"). Localizable.
#[derive(Clone, Debug, PartialEq)]
pub struct BlockInfo {
    pub name: Cow<'static, str>,
    pub display_name: Cow<'static, str>,
    pub solid: bool,
    pub opaque: bool,
    pub translucent: bool,
    pub hardness: f32,
    /// State-byte interpretation. The server consults this for placement
    /// (which `state` value to write) and physics (rotated hitboxes); the
    /// client mesher *also* consults it to pick which face slot to sample
    /// from the matching `BlockRenderInfo`.
    pub face_mapping: BlockFaceMapping,
}

impl BlockInfo {
    /// Start a new `BlockInfo` with the given internal `name` and UI
    /// `display_name`. Every other field defaults to its zero / false /
    /// neutral value; the chainable setters below override what the block
    /// actually needs.
    pub fn new(
        name: impl Into<Cow<'static, str>>,
        display_name: impl Into<Cow<'static, str>>,
    ) -> Self {
        Self {
            name: name.into(),
            display_name: display_name.into(),
            solid: false,
            opaque: false,
            translucent: false,
            hardness: 0.0,
            face_mapping: BlockFaceMapping::Static,
        }
    }

    /// Build the registry's reserved empty entry — `name = "empty"`,
    /// `display_name = ""`, every other field default. Inserted at
    /// `BlockId::EMPTY` by `BlockRegistry::new` and **never overwritten**
    /// by `register_base_blocks`.
    pub fn empty() -> Self {
        Self::new("empty", "")
    }

    /// Set the `solid` flag (player collides with this block).
    pub fn solid(mut self, v: bool) -> Self {
        self.solid = v;
        self
    }

    /// Set the `opaque` flag (block hides faces of neighbouring blocks).
    /// Read by the chunk mesher when culling internal faces.
    pub fn opaque(mut self, v: bool) -> Self {
        self.opaque = v;
        self
    }

    /// Set the `translucent` flag (block uses the translucent render pass).
    pub fn translucent(mut self, v: bool) -> Self {
        self.translucent = v;
        self
    }

    /// Set the hardness (mining time).
    pub fn hardness(mut self, h: f32) -> Self {
        self.hardness = h;
        self
    }

    /// Set the [`BlockFaceMapping`] used for state-driven placement /
    /// face selection.
    pub fn face_mapping(mut self, m: BlockFaceMapping) -> Self {
        self.face_mapping = m;
        self
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_is_all_default_false() {
        let info = BlockInfo::new("test.block", "Test Block");
        assert_eq!(info.name, "test.block");
        assert_eq!(info.display_name, "Test Block");
        assert!(!info.solid);
        assert!(!info.opaque);
        assert!(!info.translucent);
        assert_eq!(info.hardness, 0.0);
        assert_eq!(info.face_mapping, BlockFaceMapping::Static);
    }

    #[test]
    fn builders_chain() {
        let info = BlockInfo::new("test.rock", "Rock")
            .solid(true)
            .opaque(true)
            .hardness(2.0);
        assert!(info.solid);
        assert!(info.opaque);
        assert_eq!(info.hardness, 2.0);
    }

    #[test]
    fn empty_has_blank_display_name() {
        let info = BlockInfo::empty();
        assert_eq!(info.name, "empty");
        assert_eq!(info.display_name, "");
        assert!(!info.solid);
    }
}
