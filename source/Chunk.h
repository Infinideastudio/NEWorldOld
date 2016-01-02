﻿#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Hitbox.h"
#include "Object.h"

namespace world{
	
	extern string worldname;
	extern brightness BRIGHTNESSMIN;
	extern brightness skylight;
	class chunk;
	chunkid getChunkID(int x, int y, int z);
	void explode(int x, int y, int z, int r, chunk* c);
	class chunk{

	private:
		block* pblocks;
		brightness* pbrightness;
		vector<Object*> objects;

	public:
		//竟然一直都没有构造函数/析构函数 还要手动调用Init...我受不了啦(╯‵□′)╯︵┻━┻
		//2333 --qiaozhanrong
		chunk(int cxi, int cyi, int czi, chunkid idi) : cx(cxi), cy(cyi), cz(czi), id(idi),
			Modified(false), Empty(false), updated(false), renderBuilt(false), loadAnim(0.0) {
			memset(vertexes, 0, sizeof(vertexes));
			memset(vbuffer, 0, sizeof(vbuffer));
		}
		int cx, cy, cz;
		Hitbox::AABB aabb;
		bool Empty = false;
		bool updated = false;
		bool renderBuilt = false;
		bool Modified = false;
		chunkid id;
		vtxCount vertexes[3];
		VBOID vbuffer[3];
		double loadAnim;
		bool visible;

		void create();
		void destroy();
		void Load();
		void Unload();
		void build();
		inline string getChunkPath() {
			//assert(Empty == false);
			std::stringstream ss;
			ss << "Worlds/" << worldname << "/chunks/chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldChunk";
			return ss.str();
		}
		inline string getObjectsPath() {
			std::stringstream ss;
			ss << "Worlds\\" << worldname << "\\objects\\chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldObjects";
			return ss.str();
		}
		inline bool fileExist(string path){
			//assert(Empty == false);
			std::fstream file;
			file.open(path, std::ios::in);
			bool ret = file.is_open();
			file.close();
			return ret;
		}
		bool LoadFromFile(); //返回true代表区块文件打开成功
		void SaveToFile();
		void buildRender();
		void destroyRender();
		inline block getblock(int x, int y, int z) {
			//»ñÈ¡Çø¿éÄÚµÄ·½¿é
			//assert(Empty == false);
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
			if (pblocks == nullptr) { DebugWarning("chunk.getblock() error: Empty pointer"); return; }
			if (x>15 || x<0 || y>15 || y<0 || z>15 || z<0) { DebugWarning("chunk.getblock() error: Out of range"); return; }
#endif
			return pblocks[(x << 8) + (y << 4) + z];
		}
		inline brightness getbrightness(int x, int y, int z){
			//»ñÈ¡Çø¿éÄÚµÄÁÁ¶È
			//assert(Empty == false);
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
			if (pbrightness == nullptr) { DebugWarning("chunk.getbrightness() error: Empty pointer"); return; }
			if (x>15 || x<0 || y>15 || y<0 || z>15 || z<0) { DebugWarning("chunk.getbrightness() error: Out of range"); return; }
#endif
			return pbrightness[(x << 8) + (y << 4) + z];
		}
		inline void setblock(int x, int y, int z, block iblock) {
			if (iblock == blocks::TNT) {
				world::explode(cx * 16 + x, cy * 16 + y, cz * 16 + z, 5, this);
				return;
			}
			pblocks[(x << 8) + (y << 4) + z] = iblock;
			Modified = true;
		}
		inline void setbrightness(int x, int y, int z, brightness ibrightness){
			//ÉèÖÃÁÁ¶È
			//assert(Empty == false);
			pbrightness[(x << 8) + (y << 4) + z] = ibrightness;
			Modified = true;
		}

		Hitbox::AABB getBaseAABB();
		Hitbox::AABB getRelativeAABB(const double& x, const double& y, const double& z);
		void calcVisible(const double& xpos, const double& ypos, const double& zpos);

	};
}
