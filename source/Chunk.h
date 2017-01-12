#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Hitbox.h"

namespace world{

	extern string worldname;
	extern brightness BRIGHTNESSMIN;
	extern brightness skylight;
	
	class chunk{
	public:
		//��Ȼһֱ��û�й��캯��/�������� ��Ҫ�ֶ�����Init...���ܲ�����(�s�F����)�s��ߩ���
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
			return std::fstream(getFileName().c_str(), std::ios::in).good();
		}
		void LoadFromFile();
		void SaveToFile();
		void buildRender();
		void destroyRender();
		block getblock(int x, int y, int z) 
        {
			//��ȡ�����ڵķ���
			return pblocks[(x << 8) + (y << 4) + z];
		}
	    brightness getbrightness(int x, int y, int z)
        {
			//��ȡ�����ڵ�����
			return pbrightness[(x << 8) + (y << 4) + z];
		}
		void setblock(int x, int y, int z, block iblock) 
        {
			//���÷���
			pblocks[(x << 8) + (y << 4) + z] = iblock;
			Modified = true;
		}
		void setbrightness(int x, int y, int z, brightness ibrightness)
        {
			//��������
			pbrightness[(x << 8) + (y << 4) + z] = ibrightness;
			Modified = true;
		}

		Hitbox::AABB getChunkAABB();
		Hitbox::AABB getRelativeAABB(double& x, double& y, double& z);
	private:
		block* pblocks;
		brightness* pbrightness;
	};
}
