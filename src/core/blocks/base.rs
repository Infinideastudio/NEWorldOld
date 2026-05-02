//! Base-game block definitions (server-safe).
//!
//! Each block is registered under the `"neworld."` namespace as its
//! internal name; the `display_name` is the free-form UI label. Texture
//! references live in `client::blocks::register_base_block_visuals`,
//! keyed by the [`BaseBlocks`] ids returned here.

use std::borrow::Cow;

use super::{BlockFaceMapping, BlockId, BlockInfo, BlockRegistry};

/// Namespace prefix applied to every base-game block's internal name.
const NS: &str = "neworld";

/// Ids assigned by [`register_base_blocks`]. Stored alongside the
/// registry by the caller.
///
/// `air` aliases [`BlockId::EMPTY`] — the registry's reserved empty slot
/// is the same cell base-game callers refer to as "air". The empty slot
/// keeps its sentinel internal name (`"empty"`); base-game code reaches
/// it via `BaseBlocks.air` rather than through `id_of("neworld.air")`
/// (which returns `None` by design).
#[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
pub struct BaseBlocks {
    pub air: BlockId,
    pub rock: BlockId,
    pub grass: BlockId,
    pub dirt: BlockId,
    pub stone: BlockId,
    pub plank: BlockId,
    pub wood: BlockId,
    pub bedrock: BlockId,
    pub leaf: BlockId,
    pub glass: BlockId,
    pub water: BlockId,
    pub lava: BlockId,
    pub glowstone: BlockId,
    pub sand: BlockId,
    pub cement: BlockId,
    pub ice: BlockId,
    pub coal: BlockId,
    pub iron: BlockId,
    pub tnt: BlockId,
}

/// Build a fully-qualified block name (`"neworld.<id>"`).
fn ns(id: &str) -> Cow<'static, str> {
    Cow::Owned(format!("{NS}.{id}"))
}

/// Register the base-game blocks in a fresh `BlockRegistry`.
///
/// Slot 0 (the registry's reserved empty entry) is left untouched.
/// `BaseBlocks.air` is set to [`BlockId::EMPTY`] so existing `base.air`
/// lookups still resolve to that slot by id.
///
/// The 18 base-game blocks (`rock` … `tnt`) are appended in the legacy
/// order under the `"neworld."` namespace so existing world saves keep
/// their numeric ids. To register the matching face textures, call
/// `client::blocks::register_base_block_visuals(&base, ...)` with the
/// result.
pub fn register_base_blocks(blocks: &mut BlockRegistry) -> BaseBlocks {
    // Slot 0 is the registry's reserved empty entry; do NOT overwrite it.
    let air = BlockId::EMPTY;

    let rock = blocks.add(
        BlockInfo::new(ns("rock"), "Rock")
            .solid(true)
            .opaque(true)
            .hardness(2.0),
    );
    let grass = blocks.add(
        BlockInfo::new(ns("grass"), "Grass")
            .solid(true)
            .opaque(true)
            .hardness(0.3),
    );
    let dirt = blocks.add(
        BlockInfo::new(ns("dirt"), "Dirt")
            .solid(true)
            .opaque(true)
            .hardness(0.3),
    );
    let stone = blocks.add(
        BlockInfo::new(ns("stone"), "Stone")
            .solid(true)
            .opaque(true)
            .hardness(1.0),
    );
    let plank = blocks.add(
        BlockInfo::new(ns("plank"), "Plank")
            .solid(true)
            .opaque(true)
            .hardness(1.0),
    );
    let wood = blocks.add(
        BlockInfo::new(ns("wood"), "Wood")
            .solid(true)
            .opaque(true)
            .hardness(2.0)
            .face_mapping(BlockFaceMapping::AxisAligned),
    );
    let bedrock = blocks.add(
        BlockInfo::new(ns("bedrock"), "Bedrock")
            .solid(true)
            .opaque(true)
            .hardness(10.0),
    );
    let leaf = blocks.add(BlockInfo::new(ns("leaf"), "Leaf").solid(true).hardness(0.2));
    let glass = blocks.add(
        BlockInfo::new(ns("glass"), "Glass")
            .solid(true)
            .hardness(0.2),
    );
    let water = blocks.add(BlockInfo::new(ns("water"), "Water").translucent(true));
    let lava = blocks.add(BlockInfo::new(ns("lava"), "Lava").translucent(true));
    let glowstone = blocks.add(
        BlockInfo::new(ns("glowstone"), "Glow Stone")
            .solid(true)
            .opaque(true)
            .hardness(1.0),
    );
    let sand = blocks.add(
        BlockInfo::new(ns("sand"), "Sand")
            .solid(true)
            .opaque(true)
            .hardness(0.2),
    );
    let cement = blocks.add(
        BlockInfo::new(ns("cement"), "Cement")
            .solid(true)
            .opaque(true)
            .hardness(3.0),
    );
    let ice = blocks.add(
        BlockInfo::new(ns("ice"), "Ice")
            .solid(true)
            .translucent(true)
            .hardness(0.2),
    );
    let coal = blocks.add(
        BlockInfo::new(ns("coal"), "Coal Block")
            .solid(true)
            .opaque(true)
            .hardness(0.2),
    );
    let iron = blocks.add(
        BlockInfo::new(ns("iron"), "Iron Block")
            .solid(true)
            .opaque(true)
            .hardness(3.0),
    );
    let tnt = blocks.add(
        BlockInfo::new(ns("tnt"), "TNT")
            .solid(true)
            .opaque(true)
            .hardness(0.2),
    );
    BaseBlocks {
        air,
        rock,
        grass,
        dirt,
        stone,
        plank,
        wood,
        bedrock,
        leaf,
        glass,
        water,
        lava,
        glowstone,
        sand,
        cement,
        ice,
        coal,
        iron,
        tnt,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fresh() -> (BlockRegistry, BaseBlocks) {
        let mut blocks = BlockRegistry::new();
        let base = register_base_blocks(&mut blocks);
        (blocks, base)
    }

    #[test]
    fn registers_19_entries_total_with_empty_slot_preserved() {
        let (blocks, base) = fresh();
        assert_eq!(blocks.len(), 19);
        assert_eq!(base.air, BlockId::EMPTY);
        assert_eq!(blocks.get(base.air).name, "empty");
        assert_eq!(blocks.get(base.air).display_name, "");
        assert!(blocks.id_of("neworld.air").is_none());
        assert_eq!(blocks.id_of("neworld.rock"), Some(base.rock));
        assert_eq!(blocks.get(base.rock).hardness, 2.0);
        assert_eq!(blocks.get(base.rock).display_name, "Rock");
    }

    #[test]
    fn legacy_id_numbering_preserved() {
        let (_, base) = fresh();
        assert_eq!(base.air, BlockId(0));
        assert_eq!(base.rock, BlockId(1));
        assert_eq!(base.grass, BlockId(2));
        assert_eq!(base.tnt, BlockId(18));
    }

    #[test]
    fn wood_uses_axis_aligned_mapping() {
        let (blocks, base) = fresh();
        let wood = blocks.get(base.wood);
        assert_eq!(wood.face_mapping, BlockFaceMapping::AxisAligned);
    }
}
