#pragma once

#include <utility>
#include <utility>
#include <Math/Vector3.h>
#include "Definitions.h"
#include "Globalization.h"

namespace Blocks {
    enum BlockID {
        ENV, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
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

    public:
        BlockType(std::string blockName, bool solid, bool opaque, bool translucent, float _hardness)
                :name(std::move(blockName)), Hardness(_hardness), Solid(solid), Opaque(opaque),
                 Translucent(translucent) { };

        virtual  ~BlockType() noexcept;

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

        //方块事件
        virtual bool BeforeBlockPlace(const Int3& position, Block block) noexcept;
        virtual void AfterBlockPlace(const Int3& position, Block block) noexcept;
        virtual bool BeforeBlockDestroy(const Int3& position, Block block) noexcept;
        virtual void AfterBlockDestroy(const Int3& position, Block block) noexcept;
        virtual void OnRandomTick(const Int3& position, Block block) noexcept;
    };
}

Blocks::BlockType& BlockInfo(int id) noexcept;
