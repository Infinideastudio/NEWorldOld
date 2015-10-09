#include "chunkIndexArray.h"

namespace world{

	void chunkIndexArray::setSize(int s){
		size = s;
		size2 = size*size;
		size3 = size*size*size;
	}

	bool chunkIndexArray::create(){
		array = new int[size3];
		if (array == nullptr) return true;
		memset(array, -1, size3*sizeof(int));
		return false;
	}

	void chunkIndexArray::destroy(){
		delete[] array;
		array = nullptr;
	}

	void chunkIndexArray::move(int xd, int yd, int zd){
		int* arrTemp = new int[size3];
		for (int x = 0; x < size; x++) {
			for (int y = 0; y < size; y++) {
				for (int z = 0; z < size; z++) {
					if (elementExists(x + xd, y + yd, z + zd))arrTemp[x*size2 + y*size + z] = array[(x + xd)*size2 + (y + yd)*size + (z + zd)];
					else arrTemp[x*size2 + y*size + z] = -1;
				}
			}
		}
		delete[] array;
		array = arrTemp;
		originX += xd; originY += yd; originZ += zd;
	}

	void chunkIndexArray::moveTo(int x, int y, int z){
		move(x - originX, y - originY, z - originZ);
	}

	void chunkIndexArray::AddChunk(int ci, int cx, int cy, int cz) {
		for (int i = 0; i < size3; i++)if (array[i] >= ci) array[i]++;
		cx -= originX;
		cy -= originY;
		cz -= originZ;
		if (elementExists(cx, cy, cz)) array[cx*size2 + cy*size + cz] = ci;
	}

	void chunkIndexArray::DeleteChunk(int ci){
		for (int x = 0; x < size; x++) {
			for (int y = 0; y < size; y++) {
				for (int z = 0; z < size; z++) {
					if (array[x*size2 + y*size + z] == ci) array[x*size2 + y*size + z] = -1;
					if (array[x*size2 + y*size + z] > ci) array[x*size2 + y*size + z]--;
				}
			}
		}
	}

	bool chunkIndexArray::elementExists(int x, int y, int z){
		if (x < 0) return false;
		if (x >= size) return false;
		if (y < 0) return false;
		if (y >= size) return false;
		if (z < 0) return false;
		if (z >= size) return false;
		return true;
	}

    int chunkIndexArray::getChunkIndex(int x,int y,int z){
		x -= originX; y -= originY; z -= originZ;
		if (!elementExists(x, y, z)) return -1;
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		c_getChunkIndexFromCIA++;
#endif
		return array[x*size2 + y*size + z];
    }
}