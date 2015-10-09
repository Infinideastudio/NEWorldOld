#pragma once
#include "Definitions.h"

namespace blocks{  //方块ID
	enum{
		AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
		EOE
	};

	class SingleBlock{
	private:
		string name;
		bool Solid;
		bool Opaque;
		bool Translucent;
		bool Dark;
	public:
		SingleBlock(string blockName, bool solid, bool opaque, bool translucent) :
			name(blockName), Solid(solid), Opaque(opaque), Translucent(translucent){};

		//获得方块名称
		inline string getBlockName()const{ return name; }
		//是否是固体
		inline bool isSolid()const{ return Solid; }
		//是否不透明
		inline bool isOpaque()const{ return Opaque; }
		//是否半透明
		inline bool isTranslucent()const{ return Translucent; }
	};

	const SingleBlock blockData[EOE] = {
		//		    方块名称		  固体	 不透明	  半透明
		SingleBlock("Air"		, false	, false	, false ),
		SingleBlock("Rock"		, true	, true	, false ),
		SingleBlock("Grass"		, true	, true	, false ),
		SingleBlock("Dirt"		, true	, true	, false ),
		SingleBlock("Stone"		, true	, true	, false ),
		SingleBlock("Plank"		, true	, true	, false ),
		SingleBlock("Wood"		, true	, true	, false ),
		SingleBlock("Bedrock"	, true	, true	, false	),
		SingleBlock("Leaf"		, true	, false	, false	),
		SingleBlock("Glass"		, true	, false	, false	),
		SingleBlock("Water"		, false	, false	, true	),
		SingleBlock("Lava"		, false	, false	, true	),
		SingleBlock("GlowStone"	, true	, true	, false	),
		SingleBlock("Sand"		, true	, true	, false	),
		SingleBlock("Ice"		, true	, false	, true	),
		SingleBlock("cement"	, true	, true	, false ),
		SingleBlock("Coal Block", true	, true	, false ),
		SingleBlock("Iron Block", true	, true	, false )
	};
}
#define BlockInfo(blockID) blocks::blockData[blockID]
