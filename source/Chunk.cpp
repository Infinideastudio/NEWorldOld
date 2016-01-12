#include "Chunk.h"
#include "WorldGen.h"
#include "World.h"
#include "Blocks.h"

namespace ChunkRenderer {
	void renderChunk(World::chunk* c);
	void mergeFaceRender(World::chunk* c);
}

namespace World {

	double chunk::relBaseX, chunk::relBaseY, chunk::relBaseZ;

	void chunk::create(){
		aabb = getBaseAABB();
		pblocks = new block[4096];
		pbrightness = new brightness[4096];
		//memset(pblocks, 0, sizeof(pblocks));
		//memset(pbrightness, 0, sizeof(pbrightness));
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr){
			DebugError("Allocate memory failed!");
		}
#endif
	}

	void chunk::destroy(){
		delete[] pblocks;
		delete[] pbrightness;
		pblocks = nullptr;
		pbrightness = nullptr;
		updated = false;
		unloadedChunks++;
	}

	void chunk::build() {
		//Éú³ÉµØÐÎ
		//assert(Empty == false);
		
		int x, z, h = 0, sh = 0, wh = 0;
		int mn, mx, cur_br;
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr) {
			DebugWarning("Empty pointer when chunk generating!");
			return;
		}
#endif
		Empty = true;

		memset(pblocks, 0, 4096 * sizeof(block));
		if (cy > 8) for (int index = 0; index < 4096; index++) pbrightness[index] = skylight;
		else if (cy < 0) for (int index = 0; index < 4096; index++) pbrightness[index] = BRIGHTNESSMIN;
		else {
			memset(pbrightness, 0, 4096 * sizeof(brightness));
			int hm[16][16];
			for (x = 0; x < 16; x++) {
				for (z = 0; z < 16; z++) {
					hm[x][z] = HMap.getHeight(cx * 16 + x, cz * 16 + z);
				}
			}
			sh = WorldGen::WaterLevel + 2 - cy * 16;
			wh = WorldGen::WaterLevel - cy * 16;

			for (x = 0; x < 16; x++) {
				for (z = 0; z < 16; z++) {
					h = hm[x][z] - cy*16;
					if (h >= 0) Empty = false;
					if (h > sh && h > wh + 1) {
						//Grass layer
						if (h >= 0 && h < 16) pblocks[x * 256 + h * 16 + z] = Blocks::GRASS;
						//Dirt layer
						mn = min(max(0, h - 5), 16); mx = min(max(0, h), 16);
						for (int y = mn; y < mx; y++) pblocks[x * 256 + y * 16 + z] = Blocks::DIRT;
					}
					else {
						//Sand layer
						mn = min(max(0, h - 5), 16); mx = min(max(0, h + 1), 16);
						for (int y = mn; y < mx; y++) pblocks[x * 256 + y * 16 + z] = Blocks::SAND;
						//Water layer
						mn = min(max(0, h + 1), 16); mx = min(max(0, wh + 1), 16);
						cur_br = BRIGHTNESSMAX - (WorldGen::WaterLevel - (mx - 1 + cy * 16)) * 2;
						if (cur_br < BRIGHTNESSMIN) cur_br = BRIGHTNESSMIN;
						for (int y = mx - 1; y >= mn; y--) {
							pblocks[x * 256 + y * 16 + z] = Blocks::WATER;
							pbrightness[x * 256 + y * 16 + z] = (brightness)cur_br;
							cur_br -= 2; if (cur_br < BRIGHTNESSMIN) cur_br = BRIGHTNESSMIN;
						}
					}
					//Rock layer
					mx = min(max(0, h - 5), 16);
					for (int y = 0; y < mx; y++) pblocks[x * 256 + y * 16 + z] = Blocks::ROCK;
					//Air layer
					mn = min(max(0, max(h + 1, wh + 1)), 16);
					for (int y = mn; y < 16; y++) {
						pblocks[x * 256 + y * 16 + z] = Blocks::AIR;
						pbrightness[x * 256 + y * 16 + z] = skylight;
					}
					//Bedrock layer (overwrite)
					if (cy == 0) pblocks[x * 256 + z] = Blocks::BEDROCK;
				}
			}
		}
		
	}

	void chunk::Load() {
		//assert(Empty == false);

		create();
#ifndef NEWORLD_DEBUG_NO_FILEIO
		if (!LoadFromFile()) build();
#else
		build();
#endif
		if (!Empty) updated = true;
	}

	void chunk::Unload() {
		unloadedChunksCount++;
#ifndef NEWORLD_DEBUG_NO_FILEIO
		SaveToFile();
#endif
		destroyRender();
		destroy();
	}

	bool chunk::LoadFromFile() {
		std::ifstream file(getChunkPath(), std::ios::in | std::ios::binary);
		bool openChunkFile = file.is_open();
		file.read((char*)pblocks, 4096 * sizeof(block));
		file.read((char*)pbrightness, 4096 * sizeof(brightness));
		file.close();

		file.open(getObjectsPath(), std::ios::in | std::ios::binary);
		file.close();
		return openChunkFile;
	}

	void chunk::SaveToFile(){
		if (!Empty&&Modified) {
			std::ofstream file(getChunkPath(), std::ios::out | std::ios::binary);
			file.write((char*)pblocks, 4096 * sizeof(block));
			file.write((char*)pbrightness, 4096 * sizeof(brightness));
			file.close();
		}
		if (objects.size() != 0) {

		}
	}

	void chunk::buildRender() {
		//assert(Empty == false);

#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr){
			DebugWarning("Empty pointer when building vertex buffers!");
			return;
		}
#endif
		//½¨Á¢chunkÏÔÊ¾ÁÐ±í
		int x, y, z;
		for (x = -1; x <= 1; x++) {
			for (y = -1; y <= 1; y++) {
				for (z = -1; z <= 1; z++) {
					if (x == 0 && y == 0 && z == 0) continue;
					if (chunkOutOfBound(cx + x, cy + y, cz + z))  continue;
					if (!chunkLoaded(cx + x, cy + y, cz + z)) return;
				}
			}
		}
		
		rebuiltChunks++;
		updatedChunks++;

		if (renderBuilt == false){
			renderBuilt = true;
			loadAnim = cy * 16.0f + 16.0f;
		}
		
		if (MergeFace) ChunkRenderer::mergeFaceRender(this);
		else ChunkRenderer::renderChunk(this);

		updated = false;

	}

	void chunk::destroyRender() {
		if (!renderBuilt) return;
		if (vbuffer[0] != 0) vbuffersShouldDelete.push_back(vbuffer[0]);
		if (vbuffer[1] != 0) vbuffersShouldDelete.push_back(vbuffer[1]);
		if (vbuffer[2] != 0) vbuffersShouldDelete.push_back(vbuffer[2]);
		vbuffer[0] = vbuffer[1] = vbuffer[2] = 0;
		renderBuilt = false;
	}

	Hitbox::AABB chunk::getBaseAABB(){
		Hitbox::AABB ret;
		ret.xmin = cx * 16 - 0.5;
		ret.ymin = cy * 16 - 0.5;
		ret.zmin = cz * 16 - 0.5;
		ret.xmax = cx * 16 + 16 - 0.5;
		ret.ymax = cy * 16 + 16 - 0.5;
		ret.zmax = cz * 16 + 16 - 0.5;
		return ret;
	}

	Frustum::ChunkBox chunk::getRelativeAABB() {
		Frustum::ChunkBox ret;
		ret.xmin = (float)(aabb.xmin - relBaseX);
		ret.xmax = (float)(aabb.xmax - relBaseX);
		ret.ymin = (float)(aabb.ymin - loadAnim - relBaseY);
		ret.ymax = (float)(aabb.ymax - loadAnim - relBaseY);
		ret.zmin = (float)(aabb.zmin - relBaseZ);
		ret.zmax = (float)(aabb.zmax - relBaseZ);
		return ret;
	}
	
}
