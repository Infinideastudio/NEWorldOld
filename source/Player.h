#pragma once
#include "Definitions.h"
#include "Hitbox.h"
#include "OnlinePlayer.h"

const double g = 9.8;
const double EDrop = 0.1;
const double speedCast = 1 / 20.0;

extern bool canGliding;  //»¬Ïè
extern bool FLY;      //·ÉÐÐ
extern bool CROSS;    //´©Ç½ ¡û_¡û (Superman!)

extern double glidingMinimumSpeed;

class player {
public:
	static void init(double x, double y, double z);

	static void updatePosition();

	static void save(string worldn);
	static void load(string worldn);

	static void addItem(block itemname);
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

	static block BlockInHand;
	static ubyte itemInHand;
	static block inventorybox[4][10];
	static block inventorypcs[4][10];

	static double glidingEnergy, glidingSpeed;
	static inline bool gliding() { return glidingNow; }

};