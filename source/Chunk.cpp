#include "Chunk.h"
#include "WorldGen.h"
#include "World.h"
#include "Blocks.h"
#include "ChunkRenderer.h"
#include "Renderer.h"

namespace World {

	struct HMapManager {
		int H[16][16];
		int low, high, count;

		HMapManager(int cx, int cz) {
			int l = MAXINT, hi = WorldGen::WaterLevel, h;
			for (int x = 0; x < 16; ++x) {
				for (int z = 0; z < 16; ++z) {
					h = HMap.getHeight(cx * 16 + x, cz * 16 + z);
					if (h < l) l = h;
					if (h > hi) hi = h;
					H[x][z] = h;
				}
			}
			low = (l - 21) / 16, high = (hi + 16) / 16;
			count = 0;
		}
	};

	Chunk::Chunk(int cx, int cy, int cz, ChunkID cid) : cx(cx), cy(cy), cz(cz), cid(cid) {
		if (!loadFromFile()) build();
		if (!isEmpty) isUpdated = true;
		loadedChunks++;
	}

	Chunk::~Chunk() {
		saveToFile();
		destroyMeshes();
		loadedChunks--;
		unloadedChunks++;
	}
	
	std::string Chunk::getChunkPath() const {
		std::stringstream ss;
		ss << "worlds/" << WorldName << "/chunks/chunk_" << cx << "_" << cy << "_" << cz << ".neworldchunk";
		return ss.str();
	}

	void Chunk::buildTerrain() {
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr) {
			DebugWarning("Empty pointer when chunk generating!");
			return;
		}
#endif

		// Fast generate parts
		// Part1 out of the terrain bound
		if (cy < 0 || cy >= 16) {
			isEmpty = true;
			return;
		}

		// Part2 out of geometry area
		HMapManager cur = HMapManager(cx, cz);
		if (cy > cur.high && cy * 16 > WorldGen::WaterLevel) {
			isEmpty = true;
			return;
		}
		if (cy < cur.low) {
			std::fill(blocks.begin(), blocks.end(), Blocks::ROCK);
			std::fill(brightness.begin(), brightness.end(), (Brightness)0);
			if (cy == 0) for (int x = 0; x < 16; x++) for (int z = 0; z < 16; z++) blocks[x * 256 + z] = Blocks::BEDROCK;
			isEmpty = false;
			return;
		}

		// Normal Calc
		// Init
		std::fill(blocks.begin(), blocks.end(), Blocks::AIR); // Empty the chunk
		std::fill(brightness.begin(), brightness.end(), (Brightness)0); // Set All Brightness to 0

		int h = 0, sh = 0, wh = 0;
		int minh, maxh, cur_br;

		isEmpty = true;
		sh = WorldGen::WaterLevel + 2 - (cy << 4);
		wh = WorldGen::WaterLevel - (cy << 4);

		for (int x = 0; x < 16; ++x) {
			for (int z = 0; z < 16; ++z) {
				int base = (x << 8) + z;
				h = cur.H[x][z] - (cy << 4);
				if (h >= 0 || wh >= 0) isEmpty = false;
				if (h > sh && h > wh + 1) {
					// Grass layer
					if (h >= 0 && h < 16) blocks[(h << 4) + base] = Blocks::GRASS;
					// Dirt layer
					maxh = std::min(std::max(0, h), 16);
					for (int y = std::min(std::max(0, h - 5), 16); y < maxh; ++y) blocks[(y << 4) + base] = Blocks::DIRT;
				}
				else {
					// Sand layer
					maxh = std::min(std::max(0, h + 1), 16);
					for (int y = std::min(std::max(0, h - 5), 16); y < maxh; ++y) blocks[(y << 4) + base] = Blocks::SAND;
					// Water layer
					minh = std::min(std::max(0, h + 1), 16);
					maxh = std::min(std::max(0, wh + 1), 16);
					cur_br = MaxBrightness - (WorldGen::WaterLevel - (maxh - 1 + (cy << 4))) * 2;
					if (cur_br < MinBrightness) cur_br = MinBrightness;
					for (int y = maxh - 1; y >= minh; --y) {
						blocks[(y << 4) + base] = Blocks::WATER;
						brightness[(y << 4) + base] = (Brightness)cur_br;
						cur_br -= 2;
						if (cur_br < MinBrightness) cur_br = MinBrightness;
					}
				}
				// Rock layer
				maxh = std::min(std::max(0, h - 5), 16);
				for (int y = 0; y < maxh; ++y) blocks[(y << 4) + base] = Blocks::ROCK;
				// Air layer
				for (int y = std::min(std::max(0, std::max(h + 1, wh + 1)), 16); y < 16; ++y) {
					blocks[(y << 4) + base] = Blocks::AIR;
					brightness[(y << 4) + base] = SkyBrightness;
				}
				// Bedrock layer (overwrite)
				if (cy == 0) blocks[base] = Blocks::BEDROCK;
			}
		}
	}

	void Chunk::buildDetail() {
		int index = 0;
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				for (int z = 0; z < 16; z++) {
					// Tree
					if (blocks[index] == Blocks::GRASS && rnd() < 0.005)
						buildtree(cx * 16 + x, cy * 16 + y, cz * 16 + z);
					index++;
				}
			}
		}
	}

	void Chunk::build() {
		buildTerrain();
		// if (!Empty) buildDetail();
	}

	bool Chunk::loadFromFile() {
		bool exists = false;
#ifndef NEWORLD_DEBUG_NO_FILEIO
		std::ifstream file(getChunkPath(), std::ios::in | std::ios::binary);
		exists = file.is_open();
		if (exists) {
			file.read((char*)blocks.data(), 4096 * sizeof(BlockID));
			file.read((char*)brightness.data(), 4096 * sizeof(Brightness));
			file.read((char*)&isDetailGenerated, sizeof(bool));
		}
#endif
		return exists;
	}

	bool Chunk::saveToFile() {
		bool success = true;
#ifndef NEWORLD_DEBUG_NO_FILEIO
		if (!isEmpty && isModified) {
			std::ofstream file(getChunkPath(), std::ios::out | std::ios::binary);
			success = file.is_open();
			if (success) {
				file.write((char*)blocks.data(), 4096 * sizeof(BlockID));
				file.write((char*)brightness.data(), 4096 * sizeof(Brightness));
				file.write((char*)&isDetailGenerated, sizeof(bool));
			}
		}
#endif
		return success;
	}

	void Chunk::buildMeshes() {
		// Require neighboring chunks to be loaded
		int x, y, z;
		for (x = -1; x <= 1; x++) {
			for (y = -1; y <= 1; y++) {
				for (z = -1; z <= 1; z++) {
					if (x == 0 && y == 0 && z == 0) continue;
					if (chunkOutOfBound(cx + x, cy + y, cz + z)) continue;
					if (!chunkLoaded(cx + x, cy + y, cz + z)) return;
				}
			}
		}

		// Build new VBOs
		meshes = MergeFace ? ChunkRenderer::MergeFaceRenderChunk(*this) : ChunkRenderer::RenderChunk(*this);

		// Update flags
		if (!isMeshed) loadAnim = cy * 16.0f + 16.0f;
		isMeshed = true;
		isUpdated = false;

		meshedChunks++;
		updatedChunks++;
	}

	void Chunk::destroyMeshes() {
		meshes.clear();
		isMeshed = false;
		isUpdated = true;
	}

	Hitbox::AABB Chunk::baseAABB() const {
		Hitbox::AABB ret;
		ret.xmin = cx * 16 - 0.5;
		ret.xmax = cx * 16 + 16 - 0.5;
		ret.ymin = cy * 16 - 0.5;
		ret.ymax = cy * 16 + 16 - 0.5;
		ret.zmin = cz * 16 - 0.5;
		ret.zmax = cz * 16 + 16 - 0.5;
		return ret;
	}

	FrustumTest::AABBf Chunk::relativeAABB(Vec3d const& orig) const {
		FrustumTest::AABBf ret;
		ret.xmin = static_cast<float>(cx * 16 - 0.5 - orig.x);
		ret.xmax = static_cast<float>(cx * 16 + 16 - 0.5 - orig.x);
		ret.ymin = static_cast<float>(cy * 16 - 0.5 - loadAnim - orig.y);
		ret.ymax = static_cast<float>(cy * 16 + 16 - 0.5 - loadAnim - orig.y);
		ret.zmin = static_cast<float>(cz * 16 - 0.5 - orig.z);
		ret.zmax = static_cast<float>(cz * 16 + 16 - 0.5 - orig.z);
		return ret;
	}
}
