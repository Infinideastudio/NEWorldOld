#ifndef CHUNKRENDERER_H
#define CHUNKRENDERER_H
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
	struct QuadPrimitive_Depth {
		int x, y, z, length, direction;
		QuadPrimitive_Depth() : x(0), y(0), z(0), length(0), direction(0) {}
	};
	void RenderPrimitive(QuadPrimitive& p);
	void RenderPrimitive_Depth(QuadPrimitive_Depth& p);
	void RenderChunk(World::chunk* c);
	void MergeFaceRender(World::chunk* c);
	void RenderDepthModel(World::chunk* c);

}
#endif
