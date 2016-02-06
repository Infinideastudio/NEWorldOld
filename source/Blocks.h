#pragma once
#include "stdinclude.h"

namespace Blocks {
	enum BlockID {
		AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
		TNT, BLOCK_DEF_END
	};
	const block NONEMPTY = 1;

	class SingleBlock {
	private:
		string name;
		float Hardness;
		bool Solid;
		bool Opaque;
		bool Translucent;
		bool Dark;
		bool canexplode;

	public:
		SingleBlock(string blockName, bool solid, bool opaque, bool translucent, bool _canexplode, float _hardness) :
			name(blockName), Solid(solid), Opaque(opaque), Translucent(translucent), canexplode(_canexplode), Hardness(_hardness) {};

		//获得方块名称
		inline string getBlockName()const { return name; }
		//是否是固体
		inline bool isSolid()const { return Solid; }
		//是否不透明
		inline bool isOpaque()const { return Opaque; }
		//是否半透明
		inline bool isTranslucent()const { return Translucent; }
		//是否可以爆炸
		inline bool canExplode()const { return canexplode; }
		//获得硬度（数值越大硬度越小，最大100）
		inline float getHardness()const { return Hardness; }
	};

	const SingleBlock blockData[BLOCK_DEF_END + 1] = {
		//			方块名称		  固体	 不透明	  半透明  可以爆炸  硬度
		SingleBlock("Air"		, false	, false	, false , false,	0),
		SingleBlock("Rock"		, true	, true	, false , false,	2),
		SingleBlock("Grass"		, true	, true	, false , false,	5),
		SingleBlock("Dirt"		, true	, true	, false , false,	5),
		SingleBlock("Stone"		, true	, true	, false , false,	2),
		SingleBlock("Plank"		, true	, true	, false , false,	5),
		SingleBlock("Wood"		, true	, true	, false , false,	5),
		SingleBlock("Bedrock"	, true	, true	, false , false,	0),
		SingleBlock("Leaf"		, true	, false	, false	, false,	15),
		SingleBlock("Glass"		, true	, false	, false	, false,	30),
		SingleBlock("Water"		, false	, false	, true	, false,	0),
		SingleBlock("Lava"		, false	, false	, true	, false,	0),
		SingleBlock("GlowStone"	, true	, true	, false	, false,	10),
		SingleBlock("Sand"		, true	, true	, false	, false,	8),
		SingleBlock("Cement"	, true	, true	, false	, false,	0.5f),
		SingleBlock("Ice"		, true	, false	, true  , false,	25),
		SingleBlock("Coal Block", true	, true	, false , false,	1),
		SingleBlock("Iron Block", true	, true	, false , false,	0.5f),
		SingleBlock("TNT"		, true	, true	, false , true,		25),
		SingleBlock("Null Block", true  , true  , false , false,	0)
	};
}
#define BlockInfo(blockID) Blocks::blockData[(blockID) >= Blocks::BLOCK_DEF_END || (blockID) < 0 ? Blocks::BLOCK_DEF_END : (blockID)]