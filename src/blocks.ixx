module;

#include <array>
#include <string_view>

export module blocks;
import types;

export enum class BlockID: uint16_t {
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

export class SingleBlock {
public:
    constexpr SingleBlock(std::string_view name, bool solid, bool opaque, bool translucent, float hardness):
        name(name),
        solid(solid),
        opaque(opaque),
        translucent(translucent),
        hardness(hardness) {};

    auto getBlockName() const -> std::string_view {
        return name;
    }
    auto isSolid() const -> bool {
        return solid;
    }
    auto isOpaque() const -> bool {
        return opaque;
    }
    auto isTranslucent() const -> bool {
        return translucent;
    }
    auto getHardness() const -> float {
        return hardness;
    }

private:
    std::string_view name;
    bool solid;
    bool opaque;
    bool translucent;
    float hardness;
};

export constexpr auto blockData = std::array{
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

export constexpr auto BlockInfo(BlockID id) -> SingleBlock const& {
    auto i = static_cast<size_t>(id);
    return blockData[i >= blockData.size() ? static_cast<size_t>(BlockID::NULLBLOCK) : i];
}
