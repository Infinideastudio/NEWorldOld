#pragma once
#include <string>
#include <vector>
#include "Hitbox.h"
#include "Frustum.h"
#include "Command.h"
typedef unsigned char ubyte;
typedef unsigned int onlineid;
typedef unsigned short item;

struct ModInfo {
	std::string name;
	std::string version;
	std::string dependence;
	void* call;
};

struct PlayerData {
	Hitbox::AABB playerbox;
	std::vector<Hitbox::AABB> Hitboxes;
	double xa, ya, za, xd, yd, zd;
	double health, healthMax, healSpeed, dropDamagePerBlock;
	onlineid onlineID;
	std::string name;
	Frustum ViewFrustum;
	bool Glide;
	bool Flying;
	bool CrossWall;
	double glidingMinimumSpeed;
	bool OnGround;
	bool Running;
	bool NearWall;
	bool inWater;
	bool glidingNow;
	double speed;
	int AirJumps;
	int cxt, cyt, czt, cxtl, cytl, cztl;
	double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
	double xlookspeed, ylookspeed;
	float height;
	float heightExt;
	item BlockInHand;
	ubyte indexInHand;
	item* inventory;
	short* inventoryAmount;
	double glidingEnergy, glidingSpeed;
};