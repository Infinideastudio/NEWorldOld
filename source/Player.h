#pragma once
#include "Definitions.h"
#include "Hitbox.h"

extern bool FLY;      //����
extern bool CROSS;    //��ǽ ��_�� (Superman!)

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

	extern Hitbox::AABB playerbox;

	extern double xa, ya, za, xd, yd, zd;
	extern bool OnGround;
	extern bool Running;
	extern bool NearWall;
	extern bool inWater;

	extern double speed;
	extern int AirJumps;
	extern int cxt, cyt, czt, cxtl, cytl, cztl;
	extern double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
	extern double xlookspeed, ylookspeed;
	extern int intxpos, intypos, intzpos, intxposold, intyposold, intzposold;
	extern double renderxpos, renderypos, renderzpos;

	extern float height;
	extern float heightExt;

	extern block BlockInHand;
	extern ubyte itemInHand;
	extern block inventorybox[4][10];
	extern block inventorypcs[4][10];

}
