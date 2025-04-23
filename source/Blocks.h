#pragma once
#include "stdinclude.h"

namespace Blocks {
	enum Blocks: BlockID {
		AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
		TNT, BLOCK_DEF_END
	};
	const BlockID NONEMPTY = 1;

	class SingleBlock {
	private:
		string name;
		bool solid;
		bool opaque;
		bool translucent;

	public:
		SingleBlock(string name, bool solid, bool opaque, bool translucent) :
			name(name), solid(solid), opaque(opaque), translucent(translucent) {};

		inline string getBlockName() const { return name; }
		inline bool isSolid() const { return solid; }
		inline bool isOpaque() const { return opaque; }
		inline bool isTranslucent() const { return translucent; }
	};

	const SingleBlock blockData[BLOCK_DEF_END + 1] = {
		//          方块名称          固体     不透明    半透明
		SingleBlock("Air", false, false, false),
		SingleBlock("Rock", true, true, false),
		SingleBlock("Grass", true, true, false),
		SingleBlock("Dirt", true, true, false),
		SingleBlock("Stone", true, true, false),
		SingleBlock("Plank", true, true, false),
		SingleBlock("Wood", true, true, false),
		SingleBlock("Bedrock", true, true, false),
		SingleBlock("Leaf", true, false, false),
		SingleBlock("Glass", true, false, false),
		SingleBlock("Water", false, false, true),
		SingleBlock("Lava", false, false, true),
		SingleBlock("GlowStone", true, true, false),
		SingleBlock("Sand", true, true, false),
		SingleBlock("Cement", true, true, false),
		SingleBlock("Ice", true, false, true),
		SingleBlock("Coal Block", true, true, false),
		SingleBlock("Iron Block", true, true, false),
		SingleBlock("TNT", true, true, false),
		SingleBlock("Null Block", true, true, false),
	};
}

inline const Blocks::SingleBlock& BlockInfo(BlockID blockID) {
	return Blocks::blockData[blockID >= Blocks::BLOCK_DEF_END ? Blocks::BLOCK_DEF_END : blockID];
}
