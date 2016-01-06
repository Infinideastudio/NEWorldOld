#ifndef WORLD_H
#define WORLD_H
#include "Definitions.h"
#include "ChunkPtrArray.h"
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

	extern chunk** chunks;
	extern int loadedChunks;
	extern int chunkArraySize;
	extern chunk* cpCachePtr;
	extern chunkid cpCacheID;
	extern HeightMap HMap;
	extern chunkPtrArray cpArray;
	extern bool cpArrayAval;

	extern int cloud[128][128];
	extern int rebuiltChunks, rebuiltChunksCount;
	extern int updatedChunks, updatedChunksCount;
	extern int unloadedChunks, unloadedChunksCount;
	extern int chunkBuildRenderList[256][2];
	extern int chunkLoadList[256][4];
	extern pair<chunk*, int> chunkUnloadList[256];
	extern vector<unsigned int> vbuffersShouldDelete;
	extern int chunkBuildRenders, chunkLoads, chunkUnloads;
	extern bool* loadedChunkArray;

	void Init();

	chunk* AddChunk(int x, int y, int z);
	void DeleteChunk(int x, int y, int z);
	chunkid getChunkID(int x, int y, int z);
	bool chunkLoaded(int x, int y, int z);
	int getChunkPtrIndex(int x, int y, int z);
	chunk* getChunkPtr(int x, int y, int z);
	void ExpandChunkArray(int cc);
	void ReduceChunkArray(int cc);

	#define getchunkpos(n) ((n)>>4)
	#define getblockpos(n) ((n)&15)
	#define chunkOutOfBound(x,y,z) ((y)<-world::worldheight || (y)>world::worldheight-1 || (x)<-134217728 || (x)>134217727 || (z)<-134217728 || (z)>134217727)

	vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB& box);
	bool inWater(const Hitbox::AABB& box);

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
	inline void setChunkUpdated(int x, int y, int z, bool value){
		chunk* i = getChunkPtr(x, y, z);
		if (i != nullptr) i->updated = value;
	}
	void sortChunkBuildRenderList(int xpos, int ypos, int zpos);
	void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

	void saveAllChunks();
	void destroyAllChunks();

	void buildtree(int x, int y, int z);
}
#endif
