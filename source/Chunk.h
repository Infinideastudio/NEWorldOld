#pragma once
#include "Definitions.h"
#include "Hitbox.h"
#include "Blocks.h"
#include "FrustumTest.h"

class Object;

namespace World {

extern string worldname;
extern Brightness BRIGHTNESSMIN;
extern Brightness skylight;

class Chunk;
ChunkID getChunkID(int x, int y, int z);
void explode(int x, int y, int z, int r, Chunk* c);

class Chunk {
private:
	int cx, cy, cz;
	ChunkID cid;

	std::unique_ptr<BlockID[]> pblocks;
	std::unique_ptr<Brightness[]> pbrightness;
	std::vector<Object*> objects;

	bool isEmpty = false;
	bool isUpdated = false;
	bool isModified = false;
	bool isDetailGenerated = false;
	bool isMeshed = false;

	std::vector<std::pair<VBOID, GLuint>> meshes;
	float loadAnim = 0.0f;

	static double relBaseX, relBaseY, relBaseZ;
	static FrustumTest TestFrustum;

	void buildTerrain();
	void buildDetail();
	void build();
	std::string getChunkPath() const;
	std::string getObjectsPath() const;

public:
	Chunk(int cx, int cy, int cz, ChunkID cid);
	~Chunk();

	int x() const { return cx; }
	int y() const { return cy; }
	int z() const { return cz; }
	ChunkID id() const { return cid; }

	// Hint of content
	bool empty() const { return isEmpty; }
	// Render is dirty
	bool updated() const { return isUpdated; }
	// Disk save is dirty
	bool modified() const { return isModified; }
	// All details generated
	bool ready() const { return true; /* return isDetailGenerated; */ }
	// Meshes are available
	bool meshed() const { return isMeshed; }

	bool loadFromFile();
	bool saveToFile();
	void buildMeshes();
	void destroyMeshes();
	void markNeighborUpdated() { isUpdated = true; }

	float loadAnimOffset() const { return loadAnim; }
	void updateLoadAnimOffset() {
		if (loadAnim <= 0.3f) loadAnim = 0.0f;
		else loadAnim *= 0.6f;
	}

	std::pair<VBOID, GLuint> mesh(size_t index) const {
		assert(index < meshes.size());
		return meshes[index];
	}

	BlockID getblock(int x, int y, int z) const {
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr) { DebugWarning("chunk.getblock() error: Empty pointer"); return; }
		if (x > 15 || x < 0 || y > 15 || y < 0 || z > 15 || z < 0) { DebugWarning("chunk.getblock() error: Out of range"); return; }
#endif
		if (isEmpty) return Blocks::AIR;
		return pblocks[(x << 8) ^ (y << 4) ^ z];
	}

	Brightness getbrightness(int x, int y, int z) const {
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pbrightness == nullptr) { DebugWarning("chunk.getbrightness() error: Empty pointer"); return; }
		if (x > 15 || x < 0 || y > 15 || y < 0 || z > 15 || z < 0) { DebugWarning("chunk.getbrightness() error: Out of range"); return; }
#endif
		if (isEmpty) return cy < 0 ? BRIGHTNESSMIN : skylight;
		return pbrightness[(x << 8) ^ (y << 4) ^ z];
	}

	void setblock(int x, int y, int z, BlockID iblock) {
		if (isEmpty) {
			std::fill(pblocks.get(), pblocks.get() + 4096, Blocks::AIR);
			std::fill(pbrightness.get(), pbrightness.get() + 4096, cy < 0 ? BRIGHTNESSMIN : skylight);
			isEmpty = false;
		}
		if (iblock == Blocks::TNT) {
			World::explode(cx * 16 + x, cy * 16 + y, cz * 16 + z, 8, this);
			return;
		}
		pblocks[(x << 8) ^ (y << 4) ^ z] = iblock;
		isUpdated = true;
		isModified = true;
	}

	void setbrightness(int x, int y, int z, Brightness ibrightness) {
		if (isEmpty) {
			std::fill(pblocks.get(), pblocks.get() + 4096, Blocks::AIR);
			std::fill(pbrightness.get(), pbrightness.get() + 4096, cy < 0 ? BRIGHTNESSMIN : skylight);
			isEmpty = false;
		}
		pbrightness[(x << 8) ^ (y << 4) ^ z] = ibrightness;
		isUpdated = true;
		isModified = true;
	}

	static void setVisibilityBase(double x, double y, double z, FrustumTest const& frus) {
		relBaseX = x; relBaseY = y; relBaseZ = z; TestFrustum = frus;
	}

	Hitbox::AABB baseAABB() const;
	FrustumTest::ChunkBox relativeAABB() const;
	bool visible() const { return TestFrustum.test(relativeAABB()); }
};
}
