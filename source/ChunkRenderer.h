#pragma once
#include "Definitions.h"
#include "World.h"
#include "Renderer.h"
#include "Textures.h"

namespace ChunkRenderer {
	
	const int delta[6][3] = { { 1,0,0 },{ -1,0,0 },{ 0,1,0 },{ 0,-1,0 },{ 0,0,1 },{ 0,0,-1 } };
	void renderPrimitive(int x, int y, int z, int length, int direction, block bl, brightness br);
	void renderChunk(world::chunk* c);
	void mergeFaceRender(world::chunk* c);

}