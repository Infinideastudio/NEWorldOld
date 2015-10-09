#pragma once
#include "Definitions.h"

namespace blocks{  //����ID
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

		//��÷�������
		inline string getBlockName()const{ return name; }
		//�Ƿ��ǹ���
		inline bool isSolid()const{ return Solid; }
		//�Ƿ�͸��
		inline bool isOpaque()const{ return Opaque; }
		//�Ƿ��͸��
		inline bool isTranslucent()const{ return Translucent; }
	};

	const SingleBlock blockData[EOE] = {
		//		    ��������		  ����	 ��͸��	  ��͸��
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
