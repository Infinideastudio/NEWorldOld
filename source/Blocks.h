#pragma once
#include "Definitions.h"

namespace blocks{  //����ID
	enum{
		AIR, ROCK, GRASS, DIRT, STONE, PLANK, WOOD, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON,
		TNT,
		BLOCK_DEF_END
	};
	const block NONEMPTY = 1;

	class SingleBlock{
	private:
		string name;
		bool Solid;
		bool Opaque;
		bool Translucent;
		bool Dark;
		bool canexplode;

	public:
		SingleBlock(string blockName, bool solid, bool opaque, bool translucent, bool _canexplode) :
			name(blockName), Solid(solid), Opaque(opaque), Translucent(translucent), canexplode(_canexplode) {};

		//��÷�������
		inline string getBlockName()const{ return name; }
		//�Ƿ��ǹ���
		inline bool isSolid()const{ return Solid; }
		//�Ƿ�͸��
		inline bool isOpaque()const{ return Opaque; }
		//�Ƿ��͸��
		inline bool isTranslucent()const{ return Translucent; }
		//�Ƿ���Ա�ը
		inline bool canExplode()const { return canexplode; }
	};

	const SingleBlock blockData[BLOCK_DEF_END] = {
		//		    ��������		  ����	 ��͸��	  ��͸��  ���Ա�ը
		SingleBlock("Air"		, false	, false	, false , false),
		SingleBlock("Rock"		, true	, true	, false , false),
		SingleBlock("Grass"		, true	, true	, false , false),
		SingleBlock("Dirt"		, true	, true	, false , false),
		SingleBlock("Stone"		, true	, true	, false , false),
		SingleBlock("Plank"		, true	, true	, false , false),
		SingleBlock("Wood"		, true	, true	, false , false),
		SingleBlock("Bedrock"	, true	, true	, false , false),
		SingleBlock("Leaf"		, true	, false	, false	, false),
		SingleBlock("Glass"		, true	, false	, false	, false),
		SingleBlock("Water"		, false	, false	, true	, false),
		SingleBlock("Lava"		, false	, false	, true	, false),
		SingleBlock("GlowStone"	, true	, true	, false	, false),
		SingleBlock("Sand"		, true	, true	, false	, false),
		SingleBlock("Ice"		, true	, false	, true	, false),
		SingleBlock("cement"	, true	, true	, false , false),
		SingleBlock("Coal Block", true	, true	, false , false),
		SingleBlock("Iron Block", true	, true	, false , false),
		SingleBlock("TNT"		, true	, true	, false , true)
	};
}
#define BlockInfo(blockID) blocks::blockData[blockID]
