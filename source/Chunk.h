#ifndef CHUNK_H
#define CHUNK_H
#include "Definitions.h"
#include "Hitbox.h"
#include "Blocks.h"
#include "Frustum.h"
namespace World{
	
	extern string worldname;
	extern brightness BRIGHTNESSMIN;
	extern brightness skylight;
	chunkid getChunkID(int x, int y, int z);
	class chunk;
	void explode(int x, int y, int z, int r, chunk* c);

	class chunk{
	private:
		block* pblocks;
		brightness* pbrightness;
		static double relBaseX, relBaseY, relBaseZ;

	public:
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
		vtxCount vertexes[4];
		VBOID vbuffer[4];
		double loadAnim;
		bool visible;

		void create();
		void destroy();
		void Load();
		void Unload();
		void build();
		inline string getFileName(){
			return "Worlds/" + worldname + "/chunks/chunk_" + itos(cx) + "_" + itos(cy) + "_" + itos(cz) + ".NEWorldChunk";
		}
		inline bool fileExist(){
			return wxFile::Exists(getFileName());
		}
		bool LoadFromFile(); //返回true代表区块文件打开成功
		void SaveToFile();
		void buildRender();
		void destroyRender();
		inline block getblock(int x, int y, int z) {
			//获取区块内的方块
			if (Empty) return Blocks::AIR;
			return pblocks[(x << 8) ^ (y << 4) ^ z];
		}
		inline brightness getbrightness(int x, int y, int z){
			//获取区块内的亮度
			if (Empty)if (cy < 0)return BRIGHTNESSMIN; else return skylight;
			return pbrightness[(x << 8) ^ (y << 4) ^ z];
		}
		inline void setblock(int x, int y, int z, block iblock) {
			//设置方块
			if (iblock == Blocks::TNT) {
				World::explode(cx * 16 + x, cy * 16 + y, cz * 16 + z, 8, this);
				return;
			}
			pblocks[(x << 8) ^ (y << 4) ^ z] = iblock;
			Modified = true;
		}
		inline void setbrightness(int x, int y, int z, brightness ibrightness){
			//设置亮度
			pbrightness[(x << 8) ^ (y << 4) ^ z] = ibrightness;
			Modified = true;
		}

		static void setRelativeBase(double x, double y, double z) {
			relBaseX = x; relBaseY = y; relBaseZ = z;
		}

		Hitbox::AABB getBaseAABB();
		Frustum::ChunkBox getRelativeAABB();
		inline void calcVisible() { visible = Frustum::FrustumTest(getRelativeAABB()); }

	};
}
#endif