#pragma once
#include "Definitions.h"
#include "chunkPtrArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"
#include "Blocks.h"

extern int viewdistance;
class Frsutum;

namespace World {

struct LoadedCore {
	int cx = 0, cy = 0, cz = 0;
	size_t radius = 0;
};

extern string worldname;
const int worldsize = 134217728;
const int worldheight = 128;
extern Brightness skylight;         // Sky light level
extern Brightness BRIGHTNESSMAX;    // Maximum brightness
extern Brightness BRIGHTNESSMIN;    // Minimum brightness
extern Brightness BRIGHTNESSDEC;    // Brightness decrease
extern Chunk* EmptyChunkPtr;
extern size_t MaxChunkLoads;
extern size_t MaxChunkUnloads;
extern size_t MaxChunkMeshings;

extern std::unordered_map<ChunkID, std::unique_ptr<Chunk>> chunks;
extern Chunk* cpCachePtr;
extern ChunkID cpCacheID;
extern HeightMap HMap;
extern ChunkPtrArray cpArray;
extern LoadedCore loadedCore;

extern std::vector<std::pair<double, Chunk*>> chunkMeshingList;
extern std::vector<std::tuple<double, int, int, int>> chunkLoadList;
extern std::vector<std::tuple<double, int, int, int>> chunkUnloadList;

extern int loadedChunks;
extern int unloadedChunks;
extern int updatedChunks;
extern int meshedChunks;

void Init();

Chunk* AddChunk(int x, int y, int z);
void DeleteChunk(int x, int y, int z);
inline ChunkID getChunkID(int x, int y, int z) {
	if (y == -128) y = 0; if (y <= 0) y = abs(y) + (1LL << 7);
	if (x == -134217728) x = 0; if (x <= 0) x = abs(x) + (1LL << 27);
	if (z == -134217728) z = 0; if (z <= 0) z = abs(z) + (1LL << 27);
	return (ChunkID(y) << 56) + (ChunkID(x) << 28) + z;
}
Chunk* getChunkPtr(int x, int y, int z);

#define getchunkpos(n) ((n)>>4)
#define getblockpos(n) ((n)&15)
inline bool chunkOutOfBound(int x, int y, int z) {
	return y < -World::worldheight || y > World::worldheight - 1 ||
	       x < -134217728 || x > 134217727 || z < -134217728 || z > 134217727;
}
inline bool chunkLoaded(int x, int y, int z) {
	if (chunkOutOfBound(x, y, z))return false;
	if (getChunkPtr(x, y, z) != nullptr)return true;
	return false;
}

vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB& box);
bool inWater(const Hitbox::AABB& box);

void updateblock(int x, int y, int z, bool blockchanged, int depth = 0);
BlockID getblock(int x, int y, int z, BlockID mask = Blocks::AIR, Chunk* cptr = nullptr);
Brightness getbrightness(int x, int y, int z, Chunk* cptr = nullptr);
void setblock(int x, int y, int z, BlockID Block, Chunk* cptr = nullptr);
void setbrightness(int x, int y, int z, Brightness Brightness, Chunk* cptr = nullptr);
inline void putblock(int x, int y, int z, BlockID Block) { setblock(x, y, z, Block); }
inline void pickblock(int x, int y, int z) { setblock(x, y, z, Blocks::AIR); }

inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist) {
	if (x < px - dist || x > px + dist - 1 || y < py - dist || y > py + dist - 1 || z < pz - dist || z > pz + dist - 1) return false;
	return true;
}
bool chunkUpdated(int x, int y, int z);
void markChunkNeighborUpdated(int x, int y, int z);
void sortChunkUpdateLists(int xpos, int ypos, int zpos);

void saveAllChunks();
void destroyAllChunks();

void buildtree(int x, int y, int z);
void explode(int x, int y, int z, int r, Chunk* c = nullptr);
}
