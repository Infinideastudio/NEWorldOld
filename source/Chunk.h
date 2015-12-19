#pragma once
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
		bool Modified = false;
		block* pblocks;
		brightness* pbrightness;
		vector<Object*> objects;

	public:
		//竟然一直都没有构造函数/析构函数 还要手动调用Init...我受不了啦(╯‵□′)╯︵┻━┻
		//2333 --qiaozhanrong
		chunk(int cxi, int cyi, int czi, chunkid idi) : cx(cxi), cy(cyi), cz(czi), id(idi), Modified(false) {}
		chunk(int cxi, int cyi, int czi) : cx(cxi), cy(cyi), cz(czi), id(getChunkID(cxi, cyi, czi)), Modified(false) {}
		int cx, cy, cz;
		Hitbox::AABB aabb;
		bool Empty = false;
		bool updated = false;
		bool renderBuilt = false;
		chunkid id;
		vtxCount vertexes[3];
		VBOID vbuffer[3];
		double loadAnim = 0.0;
		bool visible;

		void create();
		void destroy();
		void Load();
		void Unload();
		void build();
		inline string getChunkPath() {
			std::stringstream ss;
			ss << "Worlds\\" << worldname << "\\chunks\\chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldChunk";
			return ss.str();
		}
		inline string getObjectsPath() {
			std::stringstream ss;
			ss << "Worlds\\" << worldname << "\\objects\\chunk_" << cx << "_" << cy << "_" << cz << ".NEWorldObjects";
			return ss.str();
		}
		inline bool fileExist(string path){
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
			if (iblock == blocks::TNT) {
				world::explode(cx * 16 + x, cy * 16 + y, cz * 16 + z, 5, this);
				return;
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

		Hitbox::AABB getBaseAABB();
		Hitbox::AABB getRelativeAABB(const double& x, const double& y, const double& z);
		void calcVisible(const double& xpos, const double& ypos, const double& zpos);

	};
}
