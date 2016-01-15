#pragma once
#include "Definitions.h"
#include "Blocks.h"

namespace World { class chunk; }

namespace ChunkRenderer {
	
	const int delta[6][3] = { { 1,0,0 },{ -1,0,0 },{ 0,1,0 },{ 0,-1,0 },{ 0,0,1 },{ 0,0,-1 } };
	struct QuadPrimitive {
		int x, y, z, length, direction;
		block block;
		brightness brightness;
		QuadPrimitive() : x(0), y(0), z(0), length(0), direction(0), block(Blocks::AIR), brightness(0) {}
	};
	void renderPrimitive(QuadPrimitive& p);
	void renderChunk(World::chunk* c);
	void mergeFaceRender(World::chunk* c);

}