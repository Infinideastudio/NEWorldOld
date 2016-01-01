#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Hitbox.h"

namespace world{

	extern string worldname;
	extern brightness BRIGHTNESSMIN;
	extern brightness skylight;
	chunkid getChunkID(int x, int y, int z);
	
	class chunk{
	private:
<<<<<<< HEAD
		bool Modified = false;
		//block pblocks[4096];
		//brightness pbrightness[4096];
=======
>>>>>>> refs/remotes/origin/0.4.10
		block* pblocks;
		brightness* pbrightness;

	public:
		//竟然一直都没有构造函数/析构函数 还要手动调用Init...我受不了啦(╯‵□′)╯︵┻━┻
		//2333 --qiaozhanrong
		chunk(int cxi, int cyi, int czi, chunkid idi) : cx(cxi), cy(cyi), cz(czi), id(idi),
			Modified(false), Empty(false), updated(false), renderBuilt(false), loadAnim(0.0) {
			memset(vertexes, 0, sizeof(vertexes));
			memset(vbuffer, 0, sizeof(vbuffer));
		}
		int cx, cy, cz;
		bool Empty = false;
		bool updated = false;
		bool renderBuilt = false;
		bool Modified = false;
		chunkid id;
		vtxCount vertexes[3];
		VBOID vbuffer[3];
		double loadAnim;

		void create();
		void destroy();
		void Load();
		void Unload();
		void build();
		inline string getFileName(){
			//assert(Empty == false);
			std::stringstream ss;
			ss << "Worlds/" << worldname << "/chunks/chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldChunk";
			return ss.str();
		}
		inline bool fileExist(){
			//assert(Empty == false);
			std::fstream file;
			file.open(getFileName().c_str(), std::ios::in);
			bool ret = file.is_open();
			file.close();
			return ret;
		}
		void LoadFromFile();
		void SaveToFile();
		void buildRender();
		void destroyRender();
		inline block getblock(int x, int y, int z) {
			//获取区块内的方块
			//assert(Empty == false);
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
			if (pblocks == nullptr) { DebugWarning("chunk.getblock() error: Empty pointer"); return; }
			if (x>15 || x<0 || y>15 || y<0 || z>15 || z<0) { DebugWarning("chunk.getblock() error: Out of range"); return; }
#endif
			return pblocks[(x << 8) + (y << 4) + z];
		}
		inline brightness getbrightness(int x, int y, int z){
			//获取区块内的亮度
			//assert(Empty == false);
#ifdef NEWORLD_DEBUG_CONSOLE_OUTPUT
			if (pbrightness == nullptr) { DebugWarning("chunk.getbrightness() error: Empty pointer"); return; }
			if (x>15 || x<0 || y>15 || y<0 || z>15 || z<0) { DebugWarning("chunk.getbrightness() error: Out of range"); return; }
#endif
			return pbrightness[(x << 8) + (y << 4) + z];
		}
		inline void setblock(int x, int y, int z, block iblock) {
			//设置方块
			//assert(Empty == false);
			pblocks[(x << 8) + (y << 4) + z] = iblock;
			Modified = true;
		}
		inline void setbrightness(int x, int y, int z, brightness ibrightness){
			//设置亮度
			//assert(Empty == false);
			pbrightness[(x << 8) + (y << 4) + z] = ibrightness;
			Modified = true;
		}

		Hitbox::AABB getChunkAABB();
		Hitbox::AABB getRelativeAABB(double& x, double& y, double& z);

	};
}
