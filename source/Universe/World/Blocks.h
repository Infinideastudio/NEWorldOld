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
                :name(std::move(blockName)), Solid(solid), Opaque(opaque), Translucent(translucent),
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

    extern BlockType blockData[BLOCK_DEF_END + 1];
}
#define BlockInfo(blockID) Blocks::blockData[(blockID) >= Blocks::BLOCK_DEF_END || (blockID) < 0 ? Blocks::BLOCK_DEF_END : (blockID)]