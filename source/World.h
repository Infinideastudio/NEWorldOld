#pragma once
#include "Definitions.h"
#include "ChunkIndexArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"

namespace world{

	extern string worldname;
	const int worldsize = 134217728;
	const int worldheight = 8;
	extern brightness skylight;         //Sky light level
	extern brightness BRIGHTNESSMAX;    //Maximum brightness
	extern brightness BRIGHTNESSMIN;    //Mimimum brightness
	extern brightness BRIGHTNESSDEC;    //Brightness decree
	extern chunk EmptyChunk;
	extern unsigned int EmptyBuffer;

	extern chunk* chunks;
	extern int loadedChunks;
	extern int chunkArraySize;
	extern int ciCacheIndex;
	extern uint64 ciCacheID;
	extern HeightMap HMap;
	extern chunkIndexArray ciArray;
	extern bool ciArrayAval;

	extern int cloud[128][128];
	extern int rebuiltChunks, rebuiltChunksCount;
	extern int updatedChunks, updatedChunksCount;
	extern int unloadedChunks, unloadedChunksCount;
	extern int chunkBuildRenderList[256][2];
	extern int chunkLoadList[256][4];
	extern int chunkUnloadList[256][4];
	extern vector<unsigned int> vbuffersShouldDelete;
	extern int chunkBuildRenders, chunkLoads, chunkUnloads;
	extern bool* loadedChunkArray;

	void Init();

	chunk* AddChunk(int x, int y, int z);
	void DeleteChunk(int index);
	uint64 getChunkID(int x, int y, int z);
	bool chunkLoaded(int x, int y, int z);
	int getChunkIndex(int x, int y, int z);
	chunk* getChunkPtr(int x, int y, int z);
	void ExpandChunkArray(int cc);
	void ReduceChunkArray(int cc);

	#define getchunkpos(n) ((n)>>4)
	#define getblockpos(n) ((n)&15)
	#define chunkOutOfBound(x,y,z) ((y)<-world::worldheight || (y)>world::worldheight-1 || (x)<-134217728 || (x)>134217727 || (z)<-134217728 || (z)>134217727)

	vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box);
	bool inWater(Hitbox::AABB box);

	void renderblock(int x, int y, int z, int chunkindex);
	void updateblock(int x, int y, int z, bool blockchanged);
	block getblock(int x, int y, int z, block mask = blocks::AIR, int cindex = -1);
	brightness getbrightness(int x, int y, int z, int cindex = -1);
	void setblock(int x, int y, int z, block Block);
	void setbrightness(int x, int y, int z, brightness Brightness);
	void putblock(int x, int y, int z, block Block);
	void pickblock(int x, int y, int z);

	bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist);
	inline bool chunkUpdated(int x, int y, int z){
		return getChunkPtr(x, y, z)->updated;
	}
	inline void setChunkUpdated(int x, int y, int z, bool value){
		int i = getChunkIndex(x, y, z);
		if (i != -1) chunks[i].updated = value;
	}
	void sortChunkBuildRenderList(int xpos, int ypos, int zpos);
	void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

	void saveAllChunks();
	void destroyAllChunks();

	void buildtree(int x, int y, int z);
}
