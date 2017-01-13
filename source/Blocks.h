#pragma once
#include "Definitions.h"
#include "Hitbox.h"

namespace blocks   //方块ID
{
    enum
    {
        AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
        GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
        NULLBLOCK
    };
    const block NONEMPTY = 1;

    class BlockType
    {
    public:
        BlockType(string blockName, bool solid, bool opaque, bool translucent, bool canexplode) :
            mName(blockName), mSolid(solid), mOpaque(opaque), mTranslucent(translucent), mCanExplode(canexplode)
        {}
        string getBlockName()const { return mName; }
        virtual bool isSolid()const noexcept { return mSolid; }
        virtual bool isOpaque()const noexcept { return mOpaque; }
        virtual bool isTranslucent()const noexcept { return mTranslucent; }
        virtual bool canExplode()const noexcept { return mCanExplode; }
    private:
        std::string mName;
        bool mSolid, mOpaque, mTranslucent, mDark, mCanExplode;
    };

    const BlockType blockData[NULLBLOCK + 1] =
    {
        //          方块名称             固体    不透明   半透明  可以爆炸
        BlockType("Air", false, false, false, false),
        BlockType("Rock", true, true, false, false),
        BlockType("Grass", true, true, false, false),
        BlockType("Dirt", true, true, false, false),
        BlockType("Stone", true, true, false, false),
        BlockType("Plank", true, true, false, false),
        BlockType("Wood", true, true, false, false),
        BlockType("Bedrock", true, true, false, false),
        BlockType("Leaf", true, false, false, false),
        BlockType("Glass", true, false, false, false),
        BlockType("Water", false, false, true, false),
        BlockType("Lava", false, false, true, false),
        BlockType("GlowStone", true, true, false, false),
        BlockType("Sand", true, true, false, false),
        BlockType("Cement", true, true, false, false),
        BlockType("Ice", true, false, true, false),
        BlockType("Coal Block", true, true, false, false),
        BlockType("Iron Block", true, true, false, false),
        BlockType("Null Block", true, true, false, false)
    };
}
#define BlockInfo(blockID) blocks::blockData[blockID>=blocks::NULLBLOCK?blocks::NULLBLOCK:blockID]
