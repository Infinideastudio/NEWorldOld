#pragma once
#include "StdInclude.h"
#include "Typedefs.h"

namespace Blocks {
enum Blocks : BlockID {
    AIR,
    ROCK,
    GRASS,
    DIRT,
    STONE,
    PLANK,
    WOOD,
    BEDROCK,
    LEAF,
    GLASS,
    WATER,
    LAVA,
    GLOWSTONE,
    SAND,
    CEMENT,
    ICE,
    COAL,
    IRON,
    TNT,
    NULLBLOCK,
    BLOCKS_COUNT
};
const BlockID NONEMPTY = 1;

class SingleBlock {
private:
    std::string name;
    bool solid;
    bool opaque;
    bool translucent;
    float hardness;

public:
    SingleBlock(std::string name, bool solid, bool opaque, bool translucent, float hardness):
        name(name),
        solid(solid),
        opaque(opaque),
        translucent(translucent),
        hardness(hardness) {};

    inline std::string getBlockName() const {
        return name;
    }
    inline bool isSolid() const {
        return solid;
    }
    inline bool isOpaque() const {
        return opaque;
    }
    inline bool isTranslucent() const {
        return translucent;
    }
    inline float getHardness() const {
        return hardness;
    }
};

const SingleBlock blockData[BLOCKS_COUNT] = {
    // 方块名称 固体 不透明 半透明 硬度
    SingleBlock("air", false, false, false, 0.0f),      SingleBlock("rock", true, true, false, 2.0f),
    SingleBlock("grass", true, true, false, 0.3f),      SingleBlock("dirt", true, true, false, 0.3f),
    SingleBlock("stone", true, true, false, 1.0f),      SingleBlock("plank", true, true, false, 1.0f),
    SingleBlock("wood", true, true, false, 2.0f),       SingleBlock("bedrock", true, true, false, 10.0f),
    SingleBlock("leaf", true, false, false, 0.2f),      SingleBlock("glass", true, false, false, 0.2f),
    SingleBlock("water", false, false, true, 0.0f),     SingleBlock("lava", false, false, true, 0.0f),
    SingleBlock("glow stone", true, true, false, 1.0f), SingleBlock("sand", true, true, false, 0.2f),
    SingleBlock("cement", true, true, false, 3.0f),     SingleBlock("ice", true, false, true, 0.2f),
    SingleBlock("coal block", true, true, false, 0.2f), SingleBlock("iron block", true, true, false, 3.0f),
    SingleBlock("tnt", true, true, false, 0.2f),        SingleBlock("null block", true, true, false, 0.2f),
};
}

inline const Blocks::SingleBlock& BlockInfo(BlockID blockID) {
    return Blocks::blockData[blockID >= Blocks::BLOCKS_COUNT ? Blocks::NULLBLOCK : blockID];
}
