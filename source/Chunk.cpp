#include "Chunk.h"
#include "WorldGen.h"
#include "World.h"
#include "Renderer.h"
#include "Frustum.h"

namespace world{

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
		//delete[] pblocks;
		//delete[] pbrightness;
		//pblocks = nullptr;
		//pbrightness = nullptr;
		updated = false;
		unloadedChunks++;
	}

	void chunk::build(){
		//Éú³ÉµØÐÎ

		int x, y, z, height, h=0, sh=0;
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
		if (pblocks == nullptr || pbrightness == nullptr){
			DebugWarning("Empty pointer when chunk generating!");
			return;
		}
#endif
		Empty = true;

		if (cy > 8) {

			for (int index = 0; index < 256; index++) {

				pblocks[index] = blocks::AIR;
				pbrightness[index] = skylight;

			}

		} else if (cy < 0) {

			for (int index = 0; index < 256; index++) {

				pblocks[index] = blocks::AIR;
				pbrightness[index] = BRIGHTNESSMIN;

			}

		} else {

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

						h = hm[x][z];
						height = cy * 16 + y;
						pbrightness[index] = 0;
						if (height == 0)
							pblocks[index] = blocks::BEDROCK;
						else if (height == h && height > sh && height > WorldGen::WaterLevel + 1)
							pblocks[index] = blocks::GRASS;
						else if (height<h && height>sh && height > WorldGen::WaterLevel + 1)
							pblocks[index] = blocks::DIRT;
						else if ((height >= sh - 5 || height >= h - 5) && height <= h && (height <= sh || height <= WorldGen::WaterLevel + 1))
							pblocks[index] = blocks::SAND;
						else if ((height < sh - 5 && height < h - 5) && height >= 1 && height <= h)
							pblocks[index] = blocks::ROCK;
						else {
							if (height <= WorldGen::WaterLevel) {
								pblocks[index] = blocks::WATER;
								if (skylight - (WorldGen::WaterLevel - height) * 2 < BRIGHTNESSMIN)
									pbrightness[index] = BRIGHTNESSMIN;
								else
									pbrightness[index] = skylight - (brightness)((WorldGen::WaterLevel - height) * 2);
							}
							else
							{
								pblocks[index] = blocks::AIR;
								pbrightness[index] = skylight;
							}
						}
						if (pblocks[index] != blocks::AIR) Empty = false;
						index++;

					}
				}
			}
		}

	}

	void chunk::Load(){
		create();
#ifndef NEWORLD_DEBUG_NO_FILEIO
		if (fileExist())LoadFromFile();
		else build();
#else
		build();
#endif
		if (Empty)destroy();
		else updated = true;
	}

	void chunk::Unload(){
		unloadedChunksCount++;
		if (Empty)return;
#ifndef NEWORLD_DEBUG_NO_FILEIO
		SaveToFile();
#endif
		destroyRender();
		destroy();
	}

	void chunk::LoadFromFile(){
		std::ifstream file(getFileName().c_str(), std::ios::in | std::ios::binary);
		file.read((char*)pblocks, 4096 * sizeof(block));
		file.read((char*)pbrightness, 4096 * sizeof(brightness));
		file.close();
	}

	void chunk::SaveToFile(){
		if (!Modified)return;
		std::ofstream file(getFileName().c_str(), std::ios::out | std::ios::binary);
		file.write((char*)pblocks, 4096 * sizeof(block));
		file.write((char*)pbrightness, 4096 * sizeof(brightness));
		file.close();
	}

	void chunk::buildRender(){
		if (Empty) return;
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

		int index;

		index = 0;
		renderer::Init();
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					if (pblocks[index] != blocks::AIR) {

						if (!BlockInfo(pblocks[index]).isTranslucent()) {

							renderblock(x, y, z, this);

						}
					}
					index++;
				}
			}
		}
		renderer::Flush(vbuffer[0], vertexes[0]);

		index = 0;
		renderer::Init();
		for (x = 0; x < 16; x++){
			for (y = 0; y < 16; y++){
				for (z = 0; z < 16; z++){
					if (pblocks[index] != blocks::AIR) {
						if (BlockInfo(pblocks[index]).isTranslucent()
							&& BlockInfo(pblocks[index]).isSolid()) {

							renderblock(x, y, z, this);

						}

					}
					index++;
				}
			}
		}
		renderer::Flush(vbuffer[1], vertexes[1]);

		index = 0;
		renderer::Init();
		for (x = 0; x < 16; x++){
			for (y = 0; y < 16; y++){
				for (z = 0; z < 16; z++){
					if (pblocks[index] != blocks::AIR) {

						if (!BlockInfo(pblocks[index]).isSolid()) {

							renderblock(x, y, z, this);

						}

					}
					index++;
				}
			}
		}
		renderer::Flush(vbuffer[2], vertexes[2]);
		updated = false;

	}

	void chunk::destroyRender(){
		if (!renderBuilt)return;
		if (vbuffer[0] != 0)vbuffersShouldDelete.push_back(vbuffer[0]);
		if (vbuffer[1] != 0)vbuffersShouldDelete.push_back(vbuffer[1]);
		if (vbuffer[2] != 0)vbuffersShouldDelete.push_back(vbuffer[2]);
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

	Hitbox::AABB chunk::getRelativeAABB(const double& x, const double& y, const double& z) {
		Hitbox::AABB ret;
		ret.xmin = aabb.xmin - x;
		ret.xmax = aabb.xmax - x;
		ret.ymin = aabb.ymin - loadAnim - y;
		ret.ymax = aabb.ymax - loadAnim - y;
		ret.zmin = aabb.zmin - z;
		ret.zmax = aabb.zmax - z;
		return ret;
	}

	void chunk::calcVisible(const double& xpos, const double& ypos, const double& zpos) {

		visible = Frustum::AABBInFrustum(getRelativeAABB(xpos, ypos, zpos));

	}
	
}
