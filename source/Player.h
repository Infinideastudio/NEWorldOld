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

namespace player{

	void InitHitbox();
	void InitPosition();
	bool putblock(int x, int y, int z, block blockname);
	void MoveHitbox(double x, double y, double z);
	void MoveHitboxToPosition();
	void Move();
	void save(string worldn);
	void load(string worldn);
	void additem(block itemname);

	PlayerPacket convertToPlayerPacket();


	extern Hitbox::AABB playerbox;

	extern double xa, ya, za, xd, yd, zd;
	extern onlineid onlineID;
	extern string name;
	extern bool OnGround;
	extern bool Running;
	extern bool NearWall;
	extern bool inWater;
	extern bool glidingNow;

	extern double speed;
	extern int AirJumps;
	extern int cxt, cyt, czt, cxtl, cytl, cztl;
	extern double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
	extern double xlookspeed, ylookspeed;
	extern int intxpos, intypos, intzpos, intxposold, intyposold, intzposold;

	extern float height;
	extern float heightExt;

	extern block BlockInHand;
	extern ubyte itemInHand;
	extern block inventorybox[4][10];
	extern block inventorypcs[4][10];

	extern double glidingEnergy, glidingSpeed;
	inline bool gliding() { return glidingNow; }

}
