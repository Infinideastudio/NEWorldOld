#include "World.h"
#include "Textures.h"
#include "Renderer.h"
#include "WorldGen.h"
#include "Particles.h"

namespace World {

std::string WorldName;
Chunk* EmptyChunkPtr;
size_t MaxChunkLoads = 64;
size_t MaxChunkUnloads = 64;
size_t MaxChunkMeshings = 16;
size_t MaxBlockUpdates = 4096;

struct LoadedCore {
	int cx = 0, cy = 0, cz = 0;
	size_t radius = 0;
};

std::unordered_map<ChunkID, std::unique_ptr<Chunk>> chunks;
ChunkID chunkPtrCacheKey = 0;
Chunk* chunkPtrCacheValue = nullptr;
ChunkPtrArray chunkPtrArray;
HeightMap heightMap;
LoadedCore loadedCore;

std::vector<std::pair<double, Chunk*>> chunkMeshingList;
std::vector<std::tuple<double, int, int, int>> chunkLoadList;
std::vector<std::tuple<double, int, int, int>> chunkUnloadList;
std::deque<std::tuple<int, int, int>> blockUpdateQueue;

int loadedChunks;
int unloadedChunks;
int updatedChunks;
int meshedChunks;
int updatedBlocks;

void init() {
	std::stringstream ss;
	ss << "worlds/" << WorldName << "/";
	_mkdir(ss.str().c_str());
	ss.clear(); ss.str("");
	ss << "worlds/" << WorldName << "/chunks";
	_mkdir(ss.str().c_str());

	// Create pointer for indicating empty chunks
	EmptyChunkPtr = reinterpret_cast<Chunk*>(-1);

	WorldGen::perlinNoiseInit(3404);

	chunkPtrArray.setSize((RenderDistance + 2) * 2);
	chunkPtrArray.create();

	heightMap.setSize((RenderDistance + 2) * 2 * 16);
	heightMap.create();
}

void destroy() {
	chunks.clear();
	chunkPtrCacheKey = 0;
	chunkPtrCacheValue = nullptr;
	chunkPtrArray.destroy();
	heightMap.destroy();
	loadedCore.radius = 0;

	chunkMeshingList.clear();
	chunkLoadList.clear();
	chunkUnloadList.clear();
	blockUpdateQueue.clear();

	loadedChunks = 0;
	meshedChunks = 0;
	updatedChunks = 0;
	unloadedChunks = 0;
	updatedBlocks = 0;
}

Chunk* addChunk(int x, int y, int z) {
	ChunkID cid = getChunkID(x, y, z);
	auto it = chunks.find(cid);
	if (it != chunks.end()) {
		printf("[Console][Error]");
		printf("Chunk (%d, %d, %d) has been loaded, when adding chunk.\n", x, y, z);
		return it->second.get();
	}

	auto c = std::make_unique<Chunk>(x, y, z, cid);
	Chunk* res = c.get();
	chunks.emplace(cid, std::move(c));

	chunkPtrCacheKey = cid;
	chunkPtrCacheValue = res;
	chunkPtrArray.setChunkPtr(x, y, z, res);
	return res;
}

void removeChunk(int x, int y, int z) {
	ChunkID cid = getChunkID(x, y, z);
	auto node = chunks.extract(cid);
	if (node.empty()) {
		printf("[Console][Error]");
		printf("Chunk (%d, %d, %d) has been unloaded, when deleting chunk.\n", x, y, z);
		return;
	}

	if (chunkPtrCacheValue == node.mapped().get()) {
		chunkPtrCacheKey = 0;
		chunkPtrCacheValue = nullptr;
	}
	chunkPtrArray.setChunkPtr(x, y, z, nullptr);

	// Shrink loaded core
	int xd = std::abs(x - loadedCore.cx);
	int yd = std::abs(y - loadedCore.cy);
	int zd = std::abs(z - loadedCore.cz);
	size_t dist = static_cast<size_t>(std::max(std::max(xd, yd), zd));
	loadedCore.radius = std::min(loadedCore.radius, dist);
}

Chunk* getChunkPtr(int x, int y, int z) {
	ChunkID cid = getChunkID(x, y, z);
	if (chunkPtrCacheKey == cid && chunkPtrCacheValue != nullptr) {
		return chunkPtrCacheValue;
	}
	Chunk* ret = chunkPtrArray.getChunkPtr(x, y, z);
	if (ret != nullptr) {
		chunkPtrCacheKey = cid;
		chunkPtrCacheValue = ret;
		return ret;
	}
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
	c_getChunkPtrFromSearch++;
#endif
	auto it = chunks.find(cid);
	if (it != chunks.end()) {
		ret = it->second.get();
		chunkPtrCacheKey = cid;
		chunkPtrCacheValue = ret;
		chunkPtrArray.setChunkPtr(x, y, z, ret);
		return ret;
	}
	return nullptr;
}

vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB& box) {
	//返回与box相交的所有方块AABB

	Hitbox::AABB blockbox;
	vector<Hitbox::AABB> Hitboxes;

	for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
		for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
			for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
				if (BlockInfo(getBlock(a, b, c)).isSolid()) {
					blockbox.xmin = a - 0.5; blockbox.xmax = a + 0.5;
					blockbox.ymin = b - 0.5; blockbox.ymax = b + 0.5;
					blockbox.zmin = c - 0.5; blockbox.zmax = c + 0.5;
					if (Hitbox::Hit(box, blockbox)) Hitboxes.push_back(blockbox);
				}
			}
		}
	}
	return Hitboxes;
}

bool inWater(const Hitbox::AABB& box) {
	Hitbox::AABB blockbox;
	for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5); a++) {
		for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5); b++) {
			for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5); c++) {
				if (getBlock(a, b, c) == Blocks::WATER || getBlock(a, b, c) == Blocks::LAVA) {
					blockbox.xmin = a - 0.5;
					blockbox.xmax = a + 0.5;
					blockbox.ymin = b - 0.5;
					blockbox.ymax = b + 0.5;
					blockbox.zmin = c - 0.5;
					blockbox.zmax = c + 0.5;
					if (Hitbox::Hit(box, blockbox)) return true;
				}
			}
		}
	}
	return false;
}

// Trigger block update
void updateBlock(int x, int y, int z, bool initial) {
	int cx = getchunkpos(x);
	int cy = getchunkpos(y);
	int cz = getchunkpos(z);

	if (chunkOutOfBound(cx, cy, cz)) return;

	int bx = getblockpos(x);
	int by = getblockpos(y);
	int bz = getblockpos(z);

	auto cptr = getChunkPtr(cx, cy, cz);
	if (cptr != nullptr) {
		if (cptr == EmptyChunkPtr) cptr = World::addChunk(cx, cy, cz);

		bool updated = initial;
		BlockID oldblock = cptr->getBlock(bx, by, bz);
		Brightness oldbrightness = cptr->getBrightness(bx, by, bz);

		bool skylighted = true;
		int yi, cyi;
		yi = y + 1; cyi = getchunkpos(yi);
		if (y < 0) skylighted = false;
		else {
			while (!chunkOutOfBound(cx, cyi + 1, cz) && chunkLoaded(cx, cyi + 1, cz) && skylighted) {
				if (BlockInfo(getBlock(x, yi, z)).isOpaque() || getBlock(x, yi, z) == Blocks::WATER)
					skylighted = false;
				yi++; cyi = getchunkpos(yi);
			}
		}

		if (oldblock == Blocks::TNT) {
			World::explode(x, y, z, 8);
			return;
		}

		if (oldblock == Blocks::GLOWSTONE || oldblock == Blocks::LAVA) {
			cptr->setBrightness(bx, by, bz, MaxBrightness);
		} else if (!BlockInfo(oldblock).isOpaque()) {
			Brightness br;
			int maxbrightness;
			BlockID blks[6] = {
				getBlock(x + 1, y, z),
				getBlock(x - 1, y, z),
				getBlock(x, y + 1, z),
				getBlock(x, y - 1, z),
				getBlock(x, y, z + 1),
				getBlock(x, y, z - 1),
			};
			Brightness brts[6] = {
				getBrightness(x + 1, y, z),
				getBrightness(x - 1, y, z),
				getBrightness(x, y + 1, z),
				getBrightness(x, y - 1, z),
				getBrightness(x, y, z + 1),
				getBrightness(x, y, z - 1),
			};

			maxbrightness = 0;
			for (int i = 0; i < 6; i++) if (brts[maxbrightness] < brts[i]) maxbrightness = i;
			br = brts[maxbrightness];

			if (blks[maxbrightness] == Blocks::WATER) {
				if (br - 2 < MinBrightness) br = MinBrightness; else br -= 2;
			} else {
				if (br - 1 < MinBrightness) br = MinBrightness; else br--;
			}

			if (skylighted) {
				if (br < SkyBrightness) br = SkyBrightness;
			}
			if (br < MinBrightness) br = MinBrightness;

			cptr->setBrightness(bx, by, bz, br);
		} else {
			cptr->setBrightness(bx, by, bz, 0);
		}

		if (oldblock != cptr->getBlock(bx, by, bz)) updated = true;
		if (oldbrightness != cptr->getBrightness(bx, by, bz)) updated = true;

		if (updated) {
			blockUpdateQueue.emplace_back(x + 1, y, z);
			blockUpdateQueue.emplace_back(x - 1, y, z);
			blockUpdateQueue.emplace_back(x, y + 1, z);
			blockUpdateQueue.emplace_back(x, y - 1, z);
			blockUpdateQueue.emplace_back(x, y, z + 1);
			blockUpdateQueue.emplace_back(x, y, z - 1);

			if (bx == 15 && cx < WorldSize - 1) markChunkNeighborUpdated(cx + 1, cy, cz);
			if (bx == 0 && cx > -WorldSize) markChunkNeighborUpdated(cx - 1, cy, cz);
			if (by == 15 && cy < WorldHeight - 1) markChunkNeighborUpdated(cx, cy + 1, cz);
			if (by == 0 && cy > -WorldHeight) markChunkNeighborUpdated(cx, cy - 1, cz);
			if (bz == 15 && cz < WorldSize - 1) markChunkNeighborUpdated(cx, cy, cz + 1);
			if (bz == 0 && cz > -WorldSize) markChunkNeighborUpdated(cx, cy, cz - 1);

			updatedBlocks++;
		}
	}
}

// Process pending block updates in queue
void updateBlocks() {
	for (size_t i = 0; i < MaxBlockUpdates; i++) {
		if (blockUpdateQueue.empty()) break;
		auto [x, y, z] = blockUpdateQueue.front();
		blockUpdateQueue.pop_front();
		updateBlock(x, y, z, false);
	}
}

// 获取方块
BlockID getBlock(int x, int y, int z, BlockID mask) {
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);
	if (chunkOutOfBound(cx, cy, cz)) return Blocks::AIR;
	auto cptr = getChunkPtr(cx, cy, cz);
	if (cptr == EmptyChunkPtr) return Blocks::AIR;
	if (cptr != nullptr) return cptr->getBlock(bx, by, bz);
	return mask;
}

// 获取亮度
Brightness getBrightness(int x, int y, int z) {
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);
	if (chunkOutOfBound(cx, cy, cz)) return SkyBrightness;
	auto cptr = getChunkPtr(cx, cy, cz);
	if (cptr == EmptyChunkPtr) return cy < 0 ? MinBrightness : SkyBrightness;
	if (cptr != nullptr) return cptr->getBrightness(bx, by, bz);
	return SkyBrightness;
}

// 设置方块
void setBlock(int x, int y, int z, BlockID value, bool update) {
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);
	if (!chunkOutOfBound(cx, cy, cz)) {
		auto cptr = getChunkPtr(cx, cy, cz);
		if (cptr == EmptyChunkPtr) cptr = addChunk(cx, cy, cz);
		if (cptr != nullptr) {
			cptr->setBlock(bx, by, bz, value);
			if (update) updateBlock(x, y, z, true);
		}
	}
}

bool chunkUpdated(int x, int y, int z) {
	auto i = getChunkPtr(x, y, z);
	if (i == nullptr || i == EmptyChunkPtr) return false;
	return i->updated();
}

void markChunkNeighborUpdated(int x, int y, int z) {
	auto i = getChunkPtr(x, y, z);
	if (i == nullptr || i == EmptyChunkPtr) return;
	i->markNeighborUpdated();
}

void sortChunkUpdateLists(int xpos, int ypos, int zpos) {
	int cxp = getchunkpos(xpos);
	int cyp = getchunkpos(ypos);
	int czp = getchunkpos(zpos);

	using LoadElem = std::tuple<double, int, int, int>;
	using UnloadElem = std::tuple<double, int, int, int>;
	using MeshingElem = std::pair<double, Chunk*>;

	std::priority_queue<LoadElem, std::vector<LoadElem>, std::less<LoadElem>> loads;
	std::priority_queue<UnloadElem, std::vector<UnloadElem>, std::greater<UnloadElem>> unloads;
	std::priority_queue<MeshingElem, std::vector<MeshingElem>, std::less<MeshingElem>> meshings;

	// Update loaded core center
	if (loadedCore.radius > 0) {
		int xd = std::abs(cxp - loadedCore.cx);
		int yd = std::abs(cyp - loadedCore.cy);
		int zd = std::abs(czp - loadedCore.cz);
		size_t dist = static_cast<size_t>(std::max(std::max(xd, yd), zd));
		loadedCore.radius -= std::min(loadedCore.radius, dist);
	}
	loadedCore.cx = cxp;
	loadedCore.cy = cyp;
	loadedCore.cz = czp;

	// Sort chunk load list by enumerating in cubical shells of increasing radii
	for (int radius = int(loadedCore.radius) + 1; radius <= RenderDistance + 1; radius++) {
		// Enumerate cubical shell with side length (dist * 2)
		for (int cx = cxp - radius; cx < cxp + radius; cx++) {
			for (int cy = cyp - radius; cy < cyp + radius; cy++) {
				// Skip interior of cubical shell
				int stride = radius * 2 - 1;
				if (cx == cxp - radius || cx == cxp + radius - 1) stride = 1;
				if (cy == cyp - radius || cy == cyp + radius - 1) stride = 1;
				// If both X and Y are interior, Z only checks two points
				for (int cz = czp - radius; cz < czp + radius; cz += stride) {
					if (chunkOutOfBound(cx, cy, cz)) continue;
					if (getChunkPtr(cx, cy, cz) == nullptr) {
						double xd = cx * 16 + 7 - xpos;
						double yd = cy * 16 + 7 - ypos;
						double zd = cz * 16 + 7 - zpos;
						double distSqr = xd * xd + yd * yd + zd * zd;
						loads.emplace(distSqr, cx, cy, cz);
						if (loads.size() > MaxChunkLoads) loads.pop();
					}
				}
			}
		}
		// Update loaded core radius for the known part
		if (loads.empty()) loadedCore.radius = radius;
		// Break if already complete
		if (loads.size() == MaxChunkLoads) break;
	}

	// Sort chunk unload and meshing lists simultaneously
	for (auto const& [_, c] : chunks) {
		int cx = c->x();
		int cy = c->y();
		int cz = c->z();
		if (!chunkInRange(cx, cy, cz, cxp, cyp, czp, RenderDistance + 1)) {
			double xd = cx * 16 + 7 - xpos;
			double yd = cy * 16 + 7 - ypos;
			double zd = cz * 16 + 7 - zpos;
			double distSqr = xd * xd + yd * yd + zd * zd;
			unloads.emplace(distSqr, cx, cy, cz);
			if (unloads.size() > MaxChunkUnloads) unloads.pop();
		}
		else if (chunkInRange(cx, cy, cz, cxp, cyp, czp, RenderDistance) && c->updated()) {
			double xd = cx * 16 + 7 - xpos;
			double yd = cy * 16 + 7 - ypos;
			double zd = cz * 16 + 7 - zpos;
			double distSqr = xd * xd + yd * yd + zd * zd;
			meshings.emplace(distSqr, c.get());
			if (meshings.size() > MaxChunkMeshings) meshings.pop();
		}
	}

	// Write results back
	chunkLoadList.clear();
	chunkUnloadList.clear();
	chunkMeshingList.clear();

	while (!loads.empty()) {
		chunkLoadList.emplace_back(loads.top());
		loads.pop();
	}
	while (!unloads.empty()) {
		chunkUnloadList.emplace_back(unloads.top());
		unloads.pop();
	}
	while (!meshings.empty()) {
		chunkMeshingList.emplace_back(meshings.top());
		meshings.pop();
	}
}

void buildtree(int x, int y, int z) {
	// block trblock = getBlock(x, y, z), tublock = getBlock(x, y - 1, z);
	int th = int(rnd() * 3) + 4;
	// if (trblock != Blocks::AIR || tublock != Blocks::GRASS) { return; }

	for (int yt = 0; yt != th; yt++)
		setBlock(x, y + yt, z, Blocks::WOOD);

	setBlock(x, y - 1, z, Blocks::DIRT);

	for (int xt = 0; xt != 5; xt++) {
		for (int zt = 0; zt != 5; zt++) {
			for (int yt = 0; yt != 2; yt++) {
				if (getBlock(x + xt - 2, y + th - 3 + yt, z + zt - 2) == Blocks::AIR) setBlock(x + xt - 2, y + th - 3 + yt, z + zt - 2, Blocks::LEAF);
			}
		}
	}

	for (int xt = 0; xt != 3; xt++) {
		for (int zt = 0; zt != 3; zt++) {
			for (int yt = 0; yt != 2; yt++) {
				if (getBlock(x + xt - 1, y + th - 1 + yt, z + zt - 1) == Blocks::AIR && abs(xt - 1) != abs(zt - 1)) setBlock(x + xt - 1, y + th - 1 + yt, z + zt - 1, Blocks::LEAF);
			}
		}
	}

	setBlock(x, y + th, z, Blocks::LEAF);
}

void explode(int x, int y, int z, int r) {
	auto blocks = std::vector<std::pair<Vec3i, BlockID>>();
	// Distroy blocks within a radius of r
	double maxdistsqr = r * r;
	for (int fx = x - r - 1; fx < x + r + 1; fx++) {
		for (int fy = y - r - 1; fy < y + r + 1; fy++) {
			for (int fz = z - r - 1; fz < z + r + 1; fz++) {
				int distsqr = (fx - x) * (fx - x) + (fy - y) * (fy - y) + (fz - z) * (fz - z);
				if (distsqr <= maxdistsqr * 0.75 || distsqr <= maxdistsqr && rnd() > (distsqr - maxdistsqr * 0.6) / (maxdistsqr * 0.4)) {
					BlockID e = World::getBlock(fx, fy, fz);
					if (!BlockInfo(e).isSolid()) continue;
					setBlock(fx, fy, fz, Blocks::AIR);
					blocks.emplace_back(Vec3i(fx, fy, fz), e);
				}
			}
		}
	}
	// Throw at most 1000 particles from the destroyed blocks
	std::ranges::shuffle(blocks.begin(), blocks.end(), std::mt19937());
	for (size_t i = 0; i < blocks.size() && i < 1000; i++) {
		auto const& [coord, e] = blocks[i];
		Particles::throwParticle(e,
			float(coord.x + rnd() - 0.5f), float(coord.y + rnd() - 0.2f), float(coord.z + rnd() - 0.5f),
			float(rnd() * 2.0f - 1.0f), float(rnd() * 2.0f - 1.0f), float(rnd() * 2.0f - 1.0f),
			float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
	}
}

void saveAllChunks() {
	for (auto const& [_, c] : chunks)	c->saveToFile();
}
}
