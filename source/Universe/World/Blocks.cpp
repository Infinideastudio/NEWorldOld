#include "Blocks.h"

namespace Blocks {
    BlockType blockData[BLOCK_DEF_END + 1] = {
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