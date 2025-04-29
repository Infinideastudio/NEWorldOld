#include "World.h"
#include "Textures.h"
#include "Renderer.h"
#include "WorldGen.h"
#include "Particles.h"

namespace World {

string WorldName;
Brightness SkyBrightness = 15;
Brightness MaxBrightness = 15;
Brightness MinBrightness = 2;
Brightness BrightnessAttenuation = 1;
Chunk* EmptyChunkPtr;
size_t MaxChunkLoads = 64;
size_t MaxChunkUnloads = 64;
size_t MaxChunkMeshings = 16;

std::unordered_map<ChunkID, std::unique_ptr<Chunk>> chunks;
Chunk* cpCachePtr = nullptr;
ChunkID cpCacheID = 0;
ChunkPtrArray cpArray;
HeightMap HMap;
LoadedCore loadedCore;

std::vector<std::pair<double, Chunk*>> chunkMeshingList;
std::vector<std::tuple<double, int, int, int>> chunkLoadList;
std::vector<std::tuple<double, int, int, int>> chunkUnloadList;

int loadedChunks;
int unloadedChunks;
int updatedChunks;
int meshedChunks;

void Init() {

	std::stringstream ss;
	ss << "Worlds/" << WorldName << "/";
	_mkdir(ss.str().c_str());
	ss.clear(); ss.str("");
	ss << "Worlds/" << WorldName << "/chunks";
	_mkdir(ss.str().c_str());

	// Create pointer for indicating empty chunks
	EmptyChunkPtr = (Chunk*)~0;

	WorldGen::perlinNoiseInit(3404);
	cpCachePtr = nullptr;
	cpCacheID = 0;

	cpArray.setSize((RenderDistance + 2) * 2);
	if (!cpArray.create())
		DebugError("Chunk Pointer Array not available because it couldn't be created.");

	HMap.setSize((RenderDistance + 2) * 2 * 16);
	HMap.create();

}

Chunk* AddChunk(int x, int y, int z) {
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

	cpCacheID = cid;
	cpCachePtr = res;
	cpArray.setChunkPtr(x, y, z, res);
	return res;
}

void DeleteChunk(int x, int y, int z) {
	ChunkID cid = getChunkID(x, y, z);
	auto node = chunks.extract(cid);
	if (node.empty()) {
		printf("[Console][Error]");
		printf("Chunk (%d, %d, %d) has been unloaded, when deleting chunk.\n", x, y, z);
		return;
	}

	if (cpCachePtr == node.mapped().get()) {
		cpCacheID = 0;
		cpCachePtr = nullptr;
	}
	cpArray.setChunkPtr(x, y, z, nullptr);

	// Shrink loaded core
	int xd = std::abs(x - loadedCore.cx);
	int yd = std::abs(y - loadedCore.cy);
	int zd = std::abs(z - loadedCore.cz);
	size_t dist = static_cast<size_t>(std::max(std::max(xd, yd), zd));
	loadedCore.radius = std::min(loadedCore.radius, dist);
}

Chunk* getChunkPtr(int x, int y, int z) {
	ChunkID cid = getChunkID(x, y, z);
	if (cpCacheID == cid && cpCachePtr != nullptr) {
		return cpCachePtr;
	}
	Chunk* ret = cpArray.getChunkPtr(x, y, z);
	if (ret != nullptr) {
		cpCacheID = cid;
		cpCachePtr = ret;
		return ret;
	}
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
	c_getChunkPtrFromSearch++;
#endif
	auto it = chunks.find(cid);
	if (it != chunks.end()) {
		ret = it->second.get();
		cpCacheID = cid;
		cpCachePtr = ret;
		cpArray.setChunkPtr(x, y, z, ret);
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
				if (BlockInfo(getblock(a, b, c)).isSolid()) {
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
				if (getblock(a, b, c) == Blocks::WATER || getblock(a, b, c) == Blocks::LAVA) {
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

void updateblock(int x, int y, int z, bool blockchanged, int depth) {
	//Blockupdate

	if (depth > 4096) return;
	depth++;

	bool updated = blockchanged;
	int cx = getchunkpos(x);
	int cy = getchunkpos(y);
	int cz = getchunkpos(z);

	if (chunkOutOfBound(cx, cy, cz)) return;

	int bx = getblockpos(x);
	int by = getblockpos(y);
	int bz = getblockpos(z);

	Chunk* cptr = getChunkPtr(cx, cy, cz);
	if (cptr != nullptr) {
		if (cptr == EmptyChunkPtr) cptr = World::AddChunk(cx, cy, cz);
		Brightness oldbrightness = cptr->getbrightness(bx, by, bz);
		bool skylighted = true;
		int yi, cyi;
		yi = y + 1; cyi = getchunkpos(yi);
		if (y < 0) skylighted = false;
		else {
			while (!chunkOutOfBound(cx, cyi + 1, cz) && chunkLoaded(cx, cyi + 1, cz) && skylighted) {
				if (BlockInfo(getblock(x, yi, z)).isOpaque() || getblock(x, yi, z) == Blocks::WATER)
					skylighted = false;
				yi++; cyi = getchunkpos(yi);
			}
		}

		if (!BlockInfo(getblock(x, y, z)).isOpaque()) {

			Brightness br;
			int maxbrightness;
			BlockID blks[7] = { 0,
			                  getblock(x, y, z + 1),    //Front face
			                  getblock(x, y, z - 1),    //Back face
			                  getblock(x + 1, y, z),    //Right face
			                  getblock(x - 1, y, z),    //Left face
			                  getblock(x, y + 1, z),    //Top face
			                  getblock(x, y - 1, z)
			                };  //Bottom face
			Brightness brts[7] = { 0,
			                       getbrightness(x, y, z + 1),    //Front face
			                       getbrightness(x, y, z - 1),    //Back face
			                       getbrightness(x + 1, y, z),    //Right face
			                       getbrightness(x - 1, y, z),    //Left face
			                       getbrightness(x, y + 1, z),    //Top face
			                       getbrightness(x, y - 1, z)
			                     };  //Bottom face
			maxbrightness = 1;
			for (int i = 2; i <= 6; i++) {
				if (brts[maxbrightness] < brts[i]) maxbrightness = i;
			}
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
			//Set brightness
			cptr->setbrightness(bx, by, bz, br);

		} else {

			//Opaque block
			cptr->setbrightness(bx, by, bz, 0);
			if (getblock(x, y, z) == Blocks::GLOWSTONE || getblock(x, y, z) == Blocks::LAVA)
				cptr->setbrightness(bx, by, bz, MaxBrightness);

		}

		if (oldbrightness != cptr->getbrightness(bx, by, bz)) updated = true;

		if (updated) {
			updateblock(x, y + 1, z, false, depth);
			updateblock(x, y - 1, z, false, depth);
			updateblock(x + 1, y, z, false, depth);
			updateblock(x - 1, y, z, false, depth);
			updateblock(x, y, z + 1, false, depth);
			updateblock(x, y, z - 1, false, depth);

			if (bx == 15 && cx < worldsize - 1) markChunkNeighborUpdated(cx + 1, cy, cz);
			if (bx == 0 && cx > -worldsize) markChunkNeighborUpdated(cx - 1, cy, cz);
			if (by == 15 && cy < worldheight - 1) markChunkNeighborUpdated(cx, cy + 1, cz);
			if (by == 0 && cy > -worldheight) markChunkNeighborUpdated(cx, cy - 1, cz);
			if (bz == 15 && cz < worldsize - 1) markChunkNeighborUpdated(cx, cy, cz + 1);
			if (bz == 0 && cz > -worldsize) markChunkNeighborUpdated(cx, cy, cz - 1);
		}
	}
}

BlockID getblock(int x, int y, int z, BlockID mask, Chunk* cptr) {
	//获取方块
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	if (chunkOutOfBound(cx, cy, cz)) return Blocks::AIR;
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);
	if (cptr != nullptr && cx == cptr->x() && cy == cptr->y() && cz == cptr->z())
		return cptr->getblock(bx, by, bz);
	Chunk* ci = getChunkPtr(cx, cy, cz);
	if (ci == EmptyChunkPtr) return Blocks::AIR;
	if (ci != nullptr) return ci->getblock(bx, by, bz);
	return mask;
}

Brightness getbrightness(int x, int y, int z, Chunk* cptr) {
	//获取亮度
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	if (chunkOutOfBound(cx, cy, cz)) return SkyBrightness;
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);
	if (cptr != nullptr && cx == cptr->x() && cy == cptr->y() && cz == cptr->z())
		return cptr->getbrightness(bx, by, bz);
	Chunk* ci = getChunkPtr(cx, cy, cz);
		if (ci == EmptyChunkPtr) if (cy < 0) return MinBrightness; else return SkyBrightness;
	if (ci != nullptr)return ci->getbrightness(bx, by, bz);
	return SkyBrightness;
}

void setblock(int x, int y, int z, BlockID Blockname, Chunk* cptr) {
	//设置方块
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);

	if (cptr != nullptr && cptr != EmptyChunkPtr && cx == cptr->x() && cy == cptr->y() && cz == cptr->z()) {
		cptr->setblock(bx, by, bz, Blockname);
		updateblock(x, y, z, true);
	}
	if (!chunkOutOfBound(cx, cy, cz)) {
		Chunk* i = getChunkPtr(cx, cy, cz);
		if (i == EmptyChunkPtr) i = AddChunk(cx, cy, cz);
		if (i != nullptr) {
			i->setblock(bx, by, bz, Blockname);
			updateblock(x, y, z, true);
		}
	}
}

void setbrightness(int x, int y, int z, Brightness Brightness, Chunk* cptr) {
	//设置亮度
	int cx = getchunkpos(x), cy = getchunkpos(y), cz = getchunkpos(z);
	int bx = getblockpos(x), by = getblockpos(y), bz = getblockpos(z);

	if (cptr != nullptr && cptr != EmptyChunkPtr && cx == cptr->x() && cy == cptr->y() && cz == cptr->z()) {
		cptr->setbrightness(bx, by, bz, Brightness);
	}
	if (!chunkOutOfBound(cx, cy, cz)) {
		Chunk* i = getChunkPtr(cx, cy, cz);
		if (i == EmptyChunkPtr) i = AddChunk(cx, cy, cz);
		if (i != nullptr) {
			i->setbrightness(bx, by, bz, Brightness);
		}
	}
}

bool chunkUpdated(int x, int y, int z) {
	Chunk* i = getChunkPtr(x, y, z);
	if (i == nullptr || i == EmptyChunkPtr) return false;
	return i->updated();
}

void markChunkNeighborUpdated(int x, int y, int z) {
	Chunk* i = getChunkPtr(x, y, z);
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

void saveAllChunks() {
#ifndef NEWORLD_DEBUG_NO_FILEIO
	for (auto const& [_, c] : chunks)	c->saveToFile();
#endif
}

void destroyAllChunks() {
	chunks.clear();
	cpArray.destroy();
	HMap.destroy();
	loadedCore.radius = 0;

	loadedChunks = 0;
	meshedChunks = 0;
	updatedChunks = 0;
	unloadedChunks = 0;

	chunkMeshingList.clear();
	chunkLoadList.clear();
	chunkUnloadList.clear();
}

void buildtree(int x, int y, int z) {
	// block trblock = getblock(x, y, z), tublock = getblock(x, y - 1, z);
	int th = int(rnd() * 3) + 4;
	// if (trblock != Blocks::AIR || tublock != Blocks::GRASS) { return; }

	for (int yt = 0; yt != th; yt++)
		setblock(x, y + yt, z, Blocks::WOOD);

	setblock(x, y - 1, z, Blocks::DIRT);

	for (int xt = 0; xt != 5; xt++) {
		for (int zt = 0; zt != 5; zt++) {
			for (int yt = 0; yt != 2; yt++) {
				if (getblock(x + xt - 2, y + th - 3 + yt, z + zt - 2) == Blocks::AIR) setblock(x + xt - 2, y + th - 3 + yt, z + zt - 2, Blocks::LEAF);
			}
		}
	}

	for (int xt = 0; xt != 3; xt++) {
		for (int zt = 0; zt != 3; zt++) {
			for (int yt = 0; yt != 2; yt++) {
				if (getblock(x + xt - 1, y + th - 1 + yt, z + zt - 1) == Blocks::AIR && abs(xt - 1) != abs(zt - 1)) setblock(x + xt - 1, y + th - 1 + yt, z + zt - 1, Blocks::LEAF);
			}
		}
	}

	setblock(x, y + th, z, Blocks::LEAF);
}

void explode(int x, int y, int z, int r, Chunk* c) {
	double maxdistsqr = r * r;
	for (int fx = x - r - 1; fx < x + r + 1; fx++) {
		for (int fy = y - r - 1; fy < y + r + 1; fy++) {
			for (int fz = z - r - 1; fz < z + r + 1; fz++) {
				int distsqr = (fx - x) * (fx - x) + (fy - y) * (fy - y) + (fz - z) * (fz - z);
				if (distsqr <= maxdistsqr * 0.75 ||
				        distsqr <= maxdistsqr && rnd() > (distsqr - maxdistsqr * 0.6) / (maxdistsqr * 0.4)) {
					BlockID e = World::getblock(fx, fy, fz);
					if (e == Blocks::AIR) continue;
					/*
					for (int j = 1; j <= 12; j++) {
						Particles::throwParticle(e,
						                         float(fx + rnd() - 0.5f), float(fy + rnd() - 0.2f), float(fz + rnd() - 0.5f),
						                         float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
						                         float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
					}
					*/
					setblock(fx, fy, fz, Blocks::AIR, c);
				}
			}
		}
	}
}
}
