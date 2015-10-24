#pragma once
#include "Definitions.h"
#include "chunkPtrArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"

namespace player {
	extern double xpos, ypos, zpos;
}

namespace world{

	struct updatedChunksItem{
		chunk* ptr;
		int x, y, z;
		updatedChunksItem(){}
		updatedChunksItem(chunk* _ptr, int _x,int _y,int _z):ptr(_ptr), x(_x),y(_y),z(_z) {}
		bool operator< (const updatedChunksItem& uci) const {
			int xd = (int)player::xpos - x, yd = (int)player::ypos - y, zd = (int)player::zpos - z;
			int xd1 = (int)player::xpos - uci.x, yd1 = (int)player::ypos - uci.y, zd1 = (int)player::zpos - uci.z;
			return sqrt(xd*xd + yd*yd + zd*zd) > sqrt(xd1*xd1 + yd1*yd1 + zd1*zd1);
		}
	};

	extern string worldname;
	const int worldsize = 134217728;
	const int worldheight = 8;
	extern brightness skylight;         //Sky light level
	extern brightness BRIGHTNESSMAX;    //Maximum brightness
	extern brightness BRIGHTNESSMIN;    //Mimimum brightness
	extern brightness BRIGHTNESSDEC;    //Brightness decree
	extern chunk EmptyChunk;
	extern unsigned int EmptyBuffer;

	extern chunk** chunks;
	extern int loadedChunks;
	extern int chunkArraySize;
	extern chunk* cpCachePtr;
	extern unsigned long long cpCacheID;
	extern HeightMap HMap;
	extern chunkPtrArray cpArray;
	extern bool cpArrayAval;

	extern int cloud[128][128];
	extern int updatedChunksCount, updatedChunksCounter;
	extern int unloadedChunksCount, unloadedChunksCounter;
	//extern int chunkBuildRenderList[256][2];
	extern vector<updatedChunksItem> updatedChunks; //更新了的区块
	extern int chunkLoadList[256][4];
	extern int chunkUnloadList[256][4];
	extern vector<unsigned int> vbuffersShouldDelete;
	extern int chunkBuildRenders, chunkLoads, chunkUnloads;
	extern bool* loadedChunkArray;

	void Init();

	chunk* AddChunk(int x, int y, int z);
	void DeleteChunk(int x, int y, int z);
	uint64 getChunkID(int x, int y, int z);
	bool chunkLoaded(int x, int y, int z);
	int getChunkPtrIndex(int x, int y, int z);
	chunk* getChunkPtr(int x, int y, int z);
	void ExpandChunkArray(int cc);
	void ReduceChunkArray(int cc);

	#define getchunkpos(n) ((n)>>4)
	#define getblockpos(n) ((n)&15)
	#define chunkOutOfBound(x,y,z) ((y)<-world::worldheight || (y)>world::worldheight-1 || (x)<-134217728 || (x)>134217727 || (z)<-134217728 || (z)>134217727)

	vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box);
	bool inWater(Hitbox::AABB box);

	void renderblock(int x, int y, int z, chunk* chunkptr);
	void updateblock(int x, int y, int z, bool blockchanged);
	block getblock(int x, int y, int z, block mask = blocks::AIR, chunk* cptr = nullptr);
	brightness getbrightness(int x, int y, int z, chunk* cptr = nullptr);
	void setblock(int x, int y, int z, block Block);
	void setbrightness(int x, int y, int z, brightness Brightness);
	void putblock(int x, int y, int z, block Block);
	void pickblock(int x, int y, int z);

	bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist);
	inline bool chunkUpdated(int x, int y, int z){
		return getChunkPtr(x, y, z)->updated;
	}
	inline void setChunkUpdated(int x, int y, int z){
		chunk* i = getChunkPtr(x, y, z);
		if (i != nullptr) {
			i->updated = true;
			updatedChunks.push_back(updatedChunksItem(i, x * 16 + 7, y * 16 + 7, z * 16 + 7));
		}
	}
	//void sortChunkBuildRenderList(int xpos, int ypos, int zpos);
	void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

	void saveAllChunks();
	void destroyAllChunks();

	void buildtree(int x, int y, int z);
}
