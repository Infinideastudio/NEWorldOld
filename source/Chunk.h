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
		bool Modified = false;
		//block pblocks[4096];
		//brightness pbrightness[4096];
		block* pblocks;
		brightness* pbrightness;

	public:
		//竟然一直都没有构造函数/析构函数 还要手动调用Init...我受不了啦(╯‵□′)╯︵┻━┻
		//2333 --qiaozhanrong
		chunk(int cxi, int cyi, int czi, chunkid idi) : cx(cxi), cy(cyi), cz(czi), id(idi), Modified(false) {}
		chunk(int cxi, int cyi, int czi) : cx(cxi), cy(cyi), cz(czi), id(getChunkID(cxi, cyi, czi)), Modified(false) {}
		int cx, cy, cz;
		bool Empty = false;
		bool updated = false;
		bool renderBuilt = false;
		chunkid id;
		vtxCount vertexes[3];
		VBOID vbuffer[3];
		double loadAnim = 0.0;

		void create();
		void destroy();
		void Load();
		void Unload();
		void build();
		inline string getFileName(){
			std::stringstream ss;
			ss << "Worlds\\" << worldname << "\\chunks\\chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldChunk";
			return ss.str();
		}
		inline bool fileExist(){
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
			if (Empty) return blocks::AIR;
			return pblocks[x * 256 + y * 16 + z];
		}
		inline brightness getbrightness(int x, int y, int z){
			//获取区块内的亮度
			if (Empty)if (cy < 0)return BRIGHTNESSMIN; else return skylight;
			return pbrightness[x * 256 + y * 16 + z];
		}
		inline void setblock(int x, int y, int z, block iblock) {
			//设置方块
			if (Empty){
				create();
				build();
				Empty = false;
			}
			pblocks[x * 256 + y * 16 + z] = iblock;
			Modified = true;
		}
		inline void setbrightness(int x, int y, int z, brightness ibrightness){
			//设置亮度
			if (Empty){
				create();
				build();
				Empty = false;
			}
			pbrightness[x * 256 + y * 16 + z] = ibrightness;
		}

		Hitbox::AABB getChunkAABB();
		Hitbox::AABB getRelativeAABB(double& x, double& y, double& z);

	};
}
