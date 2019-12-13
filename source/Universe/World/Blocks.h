#pragma once

#include <utility>
#include <utility>
#include "Definitions.h"
#include "Globalization.h"

namespace Blocks {
    enum BlockID {
        AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
        GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
        TNT, BLOCK_DEF_END
    };
    const Block NONEMPTY = 1;

    class BlockType {
    private:
        std::string name;
        float Hardness;
        bool Solid;
        bool Opaque;
        bool Translucent;
        bool Dark;

    public:
        BlockType(std::string blockName, bool solid, bool opaque, bool translucent, float _hardness)
                :name(std::move(std::move(blockName))), Solid(solid), Opaque(opaque), Translucent(translucent),
                Hardness(_hardness) {};

        [[nodiscard]] std::string_view GetId() const noexcept { return std::string_view(name); }

        //获得方块名称
        [[nodiscard]] std::string getBlockName() const { return Globalization::GetStrbyKey(name); }

        //是否是固体
        [[nodiscard]] bool isSolid() const { return Solid; }

        //是否不透明
        [[nodiscard]] bool isOpaque() const { return Opaque; }

        //是否半透明
        [[nodiscard]] bool isTranslucent() const { return Translucent; }

        //获得硬度（数值越大硬度越小，最大100）
        [[nodiscard]] float getHardness() const { return Hardness; }
    };

    const BlockType blockData[BLOCK_DEF_END + 1] = {
            //			方块名称		  固体	 不透明	  半透明  可以爆炸  硬度
            BlockType("NEWorld.Blocks.Air", false, false, false, 0),
            BlockType("NEWorld.Blocks.Rock", true, true, false, 2),
            BlockType("NEWorld.Blocks.Grass", true, true, false, 5),
            BlockType("NEWorld.Blocks.Dirt", true, true, false, 5),
            BlockType("NEWorld.Blocks.Stone", true, true, false, 2),
            BlockType("NEWorld.Blocks.Plank", true, true, false, 5),
            BlockType("NEWorld.Blocks.Wood", true, true, false, 5),
            BlockType("NEWorld.Blocks.Bedrock", true, true, false, 0),
            BlockType("NEWorld.Blocks.Leaf", true, false, false, 15),
            BlockType("NEWorld.Blocks.Glass", true, false, false, 30),
            BlockType("NEWorld.Blocks.Water", false, false, true, 0),
            BlockType("NEWorld.Blocks.Lava", false, false, true, 0),
            BlockType("NEWorld.Blocks.GlowStone", true, true, false, 10),
            BlockType("NEWorld.Blocks.Sand", true, true, false, 8),
            BlockType("NEWorld.Blocks.Cement", true, true, false, 0.5f),
            BlockType("NEWorld.Blocks.Ice", true, false, true, 25),
            BlockType("NEWorld.Blocks.Coal Block", true, true, false, 1),
            BlockType("NEWorld.Blocks.Iron Block", true, true, false, 0.5f),
            BlockType("NEWorld.Blocks.TNT", true, true, false, 25),
            BlockType("NEWorld.Blocks.Null Block", true, true, false, 0)
    };
}
#define BlockInfo(blockID) Blocks::blockData[(blockID) >= Blocks::BLOCK_DEF_END || (blockID) < 0 ? Blocks::BLOCK_DEF_END : (blockID)]