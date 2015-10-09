#pragma once
#include "Definitions.h"

namespace world{
	struct chunkIndexArray{
		int* array = nullptr;
		int originX, originY, originZ, size, size2, size3;
		void setSize(int s);
		bool create();
		void destroy();
		void move(int xd, int yd, int zd);
		void moveTo(int x, int y, int z);
		void AddChunk(int ci, int cx, int cy, int cz);
		void DeleteChunk(int ci);
		bool elementExists(int x, int y, int z);
		int getChunkIndex(int x, int y, int z);
	};
}
