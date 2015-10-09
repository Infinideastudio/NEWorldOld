#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Hitbox.h"

namespace world{

	extern string worldname;
	extern brightness BRIGHTNESSMIN;
	extern brightness skylight;

	class chunk{

	private:
		bool Modified = false;
		block* pblocks;
		brightness* pbrightness;

	public:
		int cx, cy, cz;
		bool Empty = false;
		bool updated = false;
		bool renderBuilt = false;
		unsigned long long id;
		unsigned int vbuffer[3];
		int vertexes[3];
		double loadAnim = 0.0;

		void Init(int cxi, int cyi, int czi, uint64 idi);
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
