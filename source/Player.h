#pragma once
#include "Definitions.h"
#include "Hitbox.h"
#include "OnlinePlayer.h"

namespace InfinideaStudio
{
	namespace NEWorld
	{


		const double g = 9.8;
		const double EDrop = 0.1;
		const double speedCast = 1 / 20.0;

		extern bool canGliding;  //����
		extern bool FLY;      //����
		extern bool CROSS;    //��ǽ ��_�� (Superman!)

		extern double glidingMinimumSpeed;

		class player
		{
		public:
			static void init(double x, double y, double z);

			static void updatePosition();

			static bool save(string worldn);
			static bool load(string worldn);

			static void addItem(item itemname, int amount = 1);
			static bool putBlock(int x, int y, int z, block blockname);

			static PlayerPacket convertToPlayerPacket();
			static Hitbox::AABB playerbox;
			static double xa, ya, za, xd, yd, zd;
			static double health, healthMax, healSpeed, dropDamagePerBlock;
			static onlineid onlineID;
			static string name;
			static bool OnGround;
			static bool Running;
			static bool NearWall;
			static bool inWater;
			static bool glidingNow;

			static double speed;
			static int AirJumps;
			static int cxt, cyt, czt, cxtl, cytl, cztl;
			static double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
			static double xlookspeed, ylookspeed;
			static int intxpos, intypos, intzpos, intxposold, intyposold, intzposold;

			static float height;
			static float heightExt;

			static item BlockInHand;
			static ubyte indexInHand;
			static item inventory [4] [10];
			static item inventoryAmount [4] [10];

			static double glidingEnergy, glidingSpeed;
			static inline bool gliding() { return glidingNow; }

		};
	}
}