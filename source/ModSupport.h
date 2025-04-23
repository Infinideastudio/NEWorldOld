#pragma once
#include <string>
#include <vector>
#include "Hitbox.h"
#include "FrustumTest.h"
#include "Command.h"
#include "Chunk.h"
typedef unsigned char ubyte;
typedef unsigned int OnlineID;
typedef unsigned short ItemID;
typedef unsigned short BlockID;

struct ModInfo {
	char name[128];
	char version[128];
	char dependence[128];
	void* call;
};

struct PlayerData {
	Hitbox::AABB playerbox;
	//std::vector<Hitbox::AABB> Hitboxes;
	double xa, ya, za, xd, yd, zd;
	double health, healthMax, healSpeed, dropDamage;
	OnlineID onlineID;
	//std::string name;
	FrustumTest ViewFrustum;
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
	int cxt, cyt, czt;
	double lookupdown, heading, xpos, ypos, zpos, xposold, yposold, zposold, jump;
	double xlookspeed, ylookspeed;
	float height;
	float heightExt;
	ItemID BlockInHand;
	ubyte indexInHand;
	ItemID* inventory;
	short* inventoryAmount;
	double glidingEnergy, glidingSpeed;
};

struct APIPackage {
	std::function<World::Chunk*(int cx, int cy, int cz)> getChunk;
	std::function<BlockID(int cx, int cy, int cz)> getBlock;
	std::function<void(int x, int y, int z, BlockID Block)> setBlock;
	std::function<Command*(string commandName)> getCommand;
	std::function<bool(Command command)> registerCommand;
	std::function<void*(std::string key)> getSharedData;
	std::function<void(std::string key, void* value)> setSharedData;
	std::function<PlayerData()> getPlayerData;
	std::function<void(int x, int y, int z, bool blockchanged)> updateBlock;
	std::function<void(int x, int y, int z)> markChunkNeighborUpdated;
};