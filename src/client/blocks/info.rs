//! Per-block render info + the registry that owns it.
//!
//! `BlockRenderInfo` is the client-only sibling of `core::BlockInfo`: it
//! holds the texture indices the mesher uses to pick which atlas layer to
//! sample for each face. The registry is keyed by [`BlockId`] so a render
//! entry naturally pairs with a block at the same id; out-of-range or
//! never-registered ids fall back to the empty entry at id 0.
//!
//! State-driven face selection (e.g. axis-aligned logs) consults the
//! [`BlockFaceMapping`] from the *core* `BlockInfo` — that lives in core
//! because it also drives placement / hitbox decisions a server needs to
//! make. The actual texture-slot resolution lives here.

use crate::core::blocks::{
    BlockFaceMapping, BlockId, BlockInfo, BlockState, face_axis, face_index_static,
};

use super::BlockTextureIndex;

/// Per-block face textures. `faces[0]` is the top, `faces[1]` is the side,
/// `faces[2]` is the bottom; the same `[top, side, bottom]` layout
/// `core::BlockInfo` used to carry. State-driven face logic uses the core
/// info's [`BlockFaceMapping`] to pick which slot a face direction reads.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Default)]
pub struct BlockRenderInfo {
    pub faces: [BlockTextureIndex; 3],
}

impl BlockRenderInfo {
    /// All faces sampling the same texture index.
    #[must_use]
    pub const fn uniform(idx: BlockTextureIndex) -> Self {
        Self { faces: [idx; 3] }
    }

    /// Explicit `[top, side, bottom]` per-face textures.
    #[must_use]
    pub const fn new(top: BlockTextureIndex, side: BlockTextureIndex, bottom: BlockTextureIndex) -> Self {
        Self {
            faces: [top, side, bottom],
        }
    }
}

/// Owning registry of [`BlockRenderInfo`] entries, keyed by [`BlockId`].
///
/// `Vec<BlockRenderInfo>` indexed by id; missing slots (a `BlockId` that no
/// one has called [`Self::set`] for) read as the default
/// `BlockRenderInfo` — face index 0 for every slot. The empty entry is
/// pre-installed at `BlockId::EMPTY` so an out-of-range lookup always
/// returns *something*.
#[derive(Clone, Debug, Default)]
pub struct BlockRenderRegistry {
    entries: Vec<BlockRenderInfo>,
}

impl BlockRenderRegistry {
    /// Empty registry with a single default entry pre-installed at
    /// [`BlockId::EMPTY`].
    #[must_use]
    pub fn new() -> Self {
        Self {
            entries: vec![BlockRenderInfo::default()],
        }
    }

    /// Bind `info` to `id`, growing the entry list with default entries
    /// for any intervening ids that haven't been set yet. Idempotent — a
    /// later call with the same id overwrites the prior info in place.
    pub fn set(&mut self, id: BlockId, info: BlockRenderInfo) {
        let slot = id.get() as usize;
        while self.entries.len() <= slot {
            self.entries.push(BlockRenderInfo::default());
        }
        self.entries[slot] = info;
    }

    /// Look up render info by id. Out-of-range ids fall back to the
    /// default entry at id 0 — same fallback policy as
    /// `core::BlockRegistry::get`.
    #[must_use]
    pub fn get(&self, id: BlockId) -> &BlockRenderInfo {
        self.entries
            .get(id.get() as usize)
            .unwrap_or(&self.entries[0])
    }

    /// Strict variant: returns `None` when `id` is out of range.
    #[must_use]
    pub fn try_get(&self, id: BlockId) -> Option<&BlockRenderInfo> {
        self.entries.get(id.get() as usize)
    }

    /// Texture for a specific face *index* (`0 = top`, `1 = side`,
    /// `2 = bottom`) on `id`. Returns `faces[2]` if `face >= 3` —
    /// matches the previous core-side `BlockInfo::face` clamping.
    #[must_use]
    pub fn face(&self, id: BlockId, face: usize) -> BlockTextureIndex {
        let info = self.get(id);
        info.faces.get(face).copied().unwrap_or(info.faces[2])
    }

    /// Texture for a face *direction* (`0..6`, `[+X, -X, +Y, -Y, +Z, -Z]`)
    /// given the cell's `state` and the core-side `core_info` that holds
    /// the [`BlockFaceMapping`]. The mesher's hot path.
    #[must_use]
    pub fn face_for(
        &self,
        id: BlockId,
        face_dir: usize,
        state: BlockState,
        core_info: &BlockInfo,
    ) -> BlockTextureIndex {
        match core_info.face_mapping {
            BlockFaceMapping::Static => self.face(id, face_index_static(face_dir)),
            BlockFaceMapping::AxisAligned => {
                let value = state.inline_or_zero();
                let cap_axis: u8 = match (value >> 1) % 3 {
                    1 => 0,
                    2 => 2,
                    _ => 1,
                };
                if face_axis(face_dir) == cap_axis {
                    self.face(id, 0)
                } else {
                    self.face(id, 1)
                }
            }
        }
    }

    /// Number of registered entries (including the implicit empty slot).
    #[must_use]
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// True iff only the implicit empty entry has been registered.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.entries.len() <= 1
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn empty_entry_lives_at_id_zero() {
        let r = BlockRenderRegistry::new();
        assert_eq!(r.get(BlockId::EMPTY), &BlockRenderInfo::default());
        // Out-of-range falls back to id 0.
        assert_eq!(r.get(BlockId(42)), &BlockRenderInfo::default());
    }

    #[test]
    fn set_grows_with_default_entries() {
        let mut r = BlockRenderRegistry::new();
        let info = BlockRenderInfo::uniform(BlockTextureIndex(7));
        r.set(BlockId(3), info);
        assert_eq!(r.len(), 4); // ids 0, 1, 2 default; 3 = info.
        assert_eq!(r.get(BlockId(1)), &BlockRenderInfo::default());
        assert_eq!(r.get(BlockId(3)), &info);
    }

    #[test]
    fn face_clamps_out_of_range() {
        let mut r = BlockRenderRegistry::new();
        let info = BlockRenderInfo::new(
            BlockTextureIndex(1),
            BlockTextureIndex(2),
            BlockTextureIndex(3),
        );
        r.set(BlockId(1), info);
        assert_eq!(r.face(BlockId(1), 0), BlockTextureIndex(1));
        assert_eq!(r.face(BlockId(1), 1), BlockTextureIndex(2));
        assert_eq!(r.face(BlockId(1), 2), BlockTextureIndex(3));
        assert_eq!(r.face(BlockId(1), 7), BlockTextureIndex(3)); // clamps to bottom
    }
}
