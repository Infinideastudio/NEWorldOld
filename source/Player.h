#pragma once
#include "Definitions.h"
#include "Vec3.h"
const double g = 9.8;
const double EDrop = 0.1;
const double speedCast = 1 / 20.0;

struct PlayerPacket;
namespace Hitbox { struct AABB; }
class FrustumTest;

class Player{
public:
	static void init(Vec3d pos_);
	static void spawn();

	static void updatePosition();

	static bool save(string worldn);
	static bool load(string worldn);

	static bool addItem(item itemname, short amount = 1);
	static bool putBlock(Vec3i pos, block blockname);

	//�޸���Ϸģʽ
	static void changeGameMode(int gamemode);

	static PlayerPacket convertToPlayerPacket();
	static Hitbox::AABB playerbox;
	static vector<Hitbox::AABB> Hitboxes;
	static double xa, ya, za, xd, yd, zd;
	static double health, healthMax, healSpeed, dropDamage;
	static onlineid onlineID;
	static string name;
	static FrustumTest ViewFrustum;

	enum GameMode { Survival, Creative };
	static int gamemode;
	static bool Glide;
	static bool Flying;
	static bool CrossWall;
	static double glidingMinimumSpeed;

	static bool OnGround;
	static bool Running;
	static bool NearWall;
	static bool inWater;
	static bool glidingNow;

	static double speed;
	static int AirJumps;
	static int cxt, cyt, czt, cxtl, cytl, cztl;
	static double lookupdown, heading, jump;
	static Vec3d pos,posold;
	static double xlookspeed, ylookspeed;
	static int intxpos, intypos, intzpos, intxposold, intyposold, intzposold;

	static float height;
	static float heightExt;

	static item BlockInHand;
	static ubyte indexInHand;
	static item inventory[4][10];
	static short inventoryAmount[4][10];

	static double glidingEnergy, glidingSpeed;
};