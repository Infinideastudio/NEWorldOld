//! Client-side base-block visual registration.
//!
//! Pairs with `core::blocks::register_base_blocks`: the core call registers
//! gameplay info into `BlockRegistry` and returns a `BaseBlocks` table of
//! ids; this call binds a [`BlockRenderInfo`] (face textures) to each of
//! those ids and registers the texture names with [`BlockTextureRegistry`].
//!
//! A future headless server skips this step entirely and runs with no
//! render or texture registries.

use std::borrow::Cow;

use crate::client::blocks::{
    BlockRenderInfo, BlockRenderRegistry, BlockTextureIndex, BlockTextureRegistry,
};
use crate::core::game::base_blocks::BaseBlocks;

fn faces(textures: &mut BlockTextureRegistry, names: [&str; 3]) -> [BlockTextureIndex; 3] {
    names.map(|n| textures.register(Cow::Owned(n.to_string())))
}

/// Bind face textures to every base-game block id in `base`. Texture
/// names are registered into `textures` on demand; `render` ends up with
/// one entry per base block, indexed by its id.
pub fn register_base_block_visuals(
    base: &BaseBlocks,
    render: &mut BlockRenderRegistry,
    textures: &mut BlockTextureRegistry,
) {
    // Slot 0 = `base.air` = empty. Default render info (all face indices
    // zero, no faces actually drawn) is fine — the chunk mesher never
    // emits faces for `base.air`.

    render.set(
        base.rock,
        BlockRenderInfo {
            faces: faces(textures, ["rock", "rock", "rock"]),
        },
    );
    render.set(
        base.grass,
        BlockRenderInfo {
            faces: faces(textures, ["grass_top", "grass_side", "dirt"]),
        },
    );
    render.set(
        base.dirt,
        BlockRenderInfo {
            faces: faces(textures, ["dirt", "dirt", "dirt"]),
        },
    );
    render.set(
        base.stone,
        BlockRenderInfo {
            faces: faces(textures, ["stone", "stone", "stone"]),
        },
    );
    render.set(
        base.plank,
        BlockRenderInfo {
            faces: faces(textures, ["plank", "plank", "plank"]),
        },
    );
    // Wood is `BlockFaceMapping::AxisAligned` (declared on the core info):
    // faces[0] is the cap, faces[1] the bark, faces[2] is unused.
    render.set(
        base.wood,
        BlockRenderInfo {
            faces: faces(textures, ["wood_top", "wood_side", "wood_top"]),
        },
    );
    render.set(
        base.bedrock,
        BlockRenderInfo {
            faces: faces(textures, ["bedrock", "bedrock", "bedrock"]),
        },
    );
    render.set(
        base.leaf,
        BlockRenderInfo {
            faces: faces(textures, ["leaf", "leaf", "leaf"]),
        },
    );
    render.set(
        base.glass,
        BlockRenderInfo {
            faces: faces(textures, ["glass", "glass", "glass"]),
        },
    );
    render.set(
        base.water,
        BlockRenderInfo {
            faces: faces(textures, ["water", "water", "water"]),
        },
    );
    render.set(
        base.lava,
        BlockRenderInfo {
            faces: faces(textures, ["lava", "lava", "lava"]),
        },
    );
    render.set(
        base.glowstone,
        BlockRenderInfo {
            faces: faces(textures, ["glowstone", "glowstone", "glowstone"]),
        },
    );
    render.set(
        base.sand,
        BlockRenderInfo {
            faces: faces(textures, ["sand", "sand", "sand"]),
        },
    );
    render.set(
        base.cement,
        BlockRenderInfo {
            faces: faces(textures, ["cement", "cement", "cement"]),
        },
    );
    render.set(
        base.ice,
        BlockRenderInfo {
            faces: faces(textures, ["ice", "ice", "ice"]),
        },
    );
    render.set(
        base.coal,
        BlockRenderInfo {
            faces: faces(textures, ["coal", "coal", "coal"]),
        },
    );
    render.set(
        base.iron,
        BlockRenderInfo {
            faces: faces(textures, ["iron", "iron", "iron"]),
        },
    );
    render.set(
        base.tnt,
        BlockRenderInfo {
            faces: faces(textures, ["tnt", "tnt", "tnt"]),
        },
    );
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::blocks::BlockRegistry;
    use crate::core::game::base_blocks::register_base_blocks;

    #[test]
    fn populates_render_registry_for_every_base_block() {
        let mut blocks = BlockRegistry::new();
        let base = register_base_blocks(&mut blocks);
        let mut render = BlockRenderRegistry::new();
        let mut textures = BlockTextureRegistry::new();
        register_base_block_visuals(&base, &mut render, &mut textures);

        // Render registry has an entry for every block id.
        assert_eq!(render.len(), blocks.len());
        // Grass faces resolve to the names we registered.
        let grass_top = textures.get("grass_top").unwrap();
        let grass_side = textures.get("grass_side").unwrap();
        let dirt = textures.get("dirt").unwrap();
        assert_eq!(render.face(base.grass, 0), grass_top);
        assert_eq!(render.face(base.grass, 1), grass_side);
        assert_eq!(render.face(base.grass, 2), dirt);
    }

    #[test]
    fn axis_aligned_wood_picks_cap_per_state_via_render_registry() {
        use crate::core::blocks::BlockState;
        let mut blocks = BlockRegistry::new();
        let base = register_base_blocks(&mut blocks);
        let mut render = BlockRenderRegistry::new();
        let mut textures = BlockTextureRegistry::new();
        register_base_block_visuals(&base, &mut render, &mut textures);

        let cap = textures.get("wood_top").unwrap();
        let side = textures.get("wood_side").unwrap();
        let wood_info = blocks.get(base.wood);

        for s in [BlockState::inline(0), BlockState::inline(1)] {
            assert_eq!(render.face_for(base.wood, 2, s, wood_info), cap);
            assert_eq!(render.face_for(base.wood, 3, s, wood_info), cap);
            for face in [0usize, 1, 4, 5] {
                assert_eq!(render.face_for(base.wood, face, s, wood_info), side);
            }
        }
        for s in [BlockState::inline(2), BlockState::inline(3)] {
            assert_eq!(render.face_for(base.wood, 0, s, wood_info), cap);
            assert_eq!(render.face_for(base.wood, 1, s, wood_info), cap);
        }
        for s in [BlockState::inline(4), BlockState::inline(5)] {
            assert_eq!(render.face_for(base.wood, 4, s, wood_info), cap);
            assert_eq!(render.face_for(base.wood, 5, s, wood_info), cap);
        }
    }
}
