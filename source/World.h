#pragma once
#include "Definitions.h"
#include "chunkPtrArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"
#include "Blocks.h"

namespace World {

const int WorldSize = 134217728;
const int WorldHeight = 128;
const Brightness SkyBrightness = 15; // Sky light level
const Brightness MaxBrightness = 15; // Maximum brightness
const Brightness MinBrightness = 2;  // Minimum brightness

extern std::string WorldName;
extern Chunk* EmptyChunkPtr;
extern size_t MaxChunkLoads;
extern size_t MaxChunkUnloads;
extern size_t MaxChunkMeshings;
extern size_t MaxBlockUpdates;

extern std::unordered_map<ChunkID, std::unique_ptr<Chunk>> chunks;
extern HeightMap heightMap;
extern ChunkPtrArray chunkPtrArray;

extern std::vector<std::pair<double, Chunk*>> chunkMeshingList;
extern std::vector<std::tuple<double, int, int, int>> chunkLoadList;
extern std::vector<std::tuple<double, int, int, int>> chunkUnloadList;
extern std::deque<std::tuple<int, int, int>> blockUpdateQueue;

extern int loadedChunks;
extern int unloadedChunks;
extern int updatedChunks;
extern int meshedChunks;
extern int updatedBlocks;

#define getchunkpos(n) ((n)>>4)
#define getblockpos(n) ((n)&15)

void init();
void destroy();

Chunk* addChunk(int x, int y, int z);
void removeChunk(int x, int y, int z);
inline ChunkID getChunkID(int x, int y, int z) {
	if (y == -128) y = 0; if (y <= 0) y = abs(y) + (1LL << 7);
	if (x == -134217728) x = 0; if (x <= 0) x = abs(x) + (1LL << 27);
	if (z == -134217728) z = 0; if (z <= 0) z = abs(z) + (1LL << 27);
	return (ChunkID(y) << 56) + (ChunkID(x) << 28) + z;
}
Chunk* getChunkPtr(int x, int y, int z);

inline bool chunkOutOfBound(int x, int y, int z) {
	return y < -World::WorldHeight || y > World::WorldHeight - 1 ||
	       x < -134217728 || x > 134217727 || z < -134217728 || z > 134217727;
}
inline bool chunkLoaded(int x, int y, int z) {
	if (chunkOutOfBound(x, y, z))return false;
	if (getChunkPtr(x, y, z) != nullptr)return true;
	return false;
}

vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB& box);
bool inWater(const Hitbox::AABB& box);

void updateBlock(int x, int y, int z, bool initial = true);
void updateBlocks();
BlockID getBlock(int x, int y, int z, BlockID mask = Blocks::AIR);
Brightness getBrightness(int x, int y, int z);
void setBlock(int x, int y, int z, BlockID value, bool update = true);

inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist) {
	if (x < px - dist || x > px + dist - 1 || y < py - dist || y > py + dist - 1 || z < pz - dist || z > pz + dist - 1) return false;
	return true;
}
bool chunkUpdated(int x, int y, int z);
void markChunkNeighborUpdated(int x, int y, int z);
void sortChunkUpdateLists(int xpos, int ypos, int zpos);

void buildtree(int x, int y, int z);
void explode(int x, int y, int z, int r);

void saveAllChunks();
}
