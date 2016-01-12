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
		
		int x, y, z, height, h = 0, sh = 0;
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr) {
			DebugWarning("Empty pointer when chunk generating!");
			return;
		}
#endif
		Empty = true;

		if (cy > 8) {
			memset(pblocks, 0, 4096 * sizeof(block));
			for (int index = 0; index < 4096; index++) pbrightness[index] = skylight;
		}
		else if (cy < 0) {
			memset(pblocks, 0, 4096 * sizeof(block));
			for (int index = 0; index < 4096; index++) pbrightness[index] = BRIGHTNESSMIN;
		}
		else {

			int hm[16][16];
			for (x = 0; x < 16; x++) {
				for (z = 0; z < 16; z++) {
					hm[x][z] = HMap.getHeight(cx * 16 + x, cz * 16 + z);
				}
			}
			sh = WorldGen::WaterLevel + 2;

			int index = 0;
			for (x = 0; x < 16; x++) {
				for (y = 0; y < 16; y++) {
					for (z = 0; z < 16; z++) {

						h = hm[x][z]; height = cy * 16 + y;
						pbrightness[index] = 0;
						if (height == 0)
							pblocks[index] = Blocks::BEDROCK;
						else if (height == h && height > sh && height > WorldGen::WaterLevel + 1)
							pblocks[index] = Blocks::GRASS;
						else if (height<h && height>sh && height > WorldGen::WaterLevel + 1)
							pblocks[index] = Blocks::DIRT;
						else if ((height >= sh - 5 || height >= h - 5) && height <= h && (height <= sh || height <= WorldGen::WaterLevel + 1))
							pblocks[index] = Blocks::SAND;
						else if ((height < sh - 5 && height < h - 5) && height >= 1 && height <= h)
							pblocks[index] = Blocks::ROCK;
						else {
							if (height <= WorldGen::WaterLevel) {
								pblocks[index] = Blocks::WATER;
								if (skylight - (WorldGen::WaterLevel - height) * 2 < BRIGHTNESSMIN)
									pbrightness[index] = BRIGHTNESSMIN;
								else
									pbrightness[index] = skylight - (brightness)((WorldGen::WaterLevel - height) * 2);
							}
							else
							{
								pblocks[index] = Blocks::AIR;
								pbrightness[index] = skylight;
							}
						}
						if (pblocks[index] != Blocks::AIR) Empty = false;
						index++;

					}
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
