#pragma once
#include "Definitions.h"
#include "Blocks.h"
#include "Textures.h"

namespace World { class Chunk; }

namespace ChunkRenderer {
	const int delta[6][3] = { { 1,0,0 },{ -1,0,0 },{ 0,1,0 },{ 0,-1,0 },{ 0,0,1 },{ 0,0,-1 } };

	// One face in merge face
	struct QuadPrimitive {
		int x, y, z, length, direction;
		/*
		* If the vertexes have different colors (smoothed lighting), the primitive cannot connect with others.
		* This variable also means whether the vertexes have different colors.
		*/
		bool once;
		// Vertex colors
		int col0, col1, col2, col3;
		// Block ID
		BlockID blk;
		// Texture ID
		TextureID tex;
		QuadPrimitive() : x(0), y(0), z(0), length(0), direction(0), once(false),
			blk(Blocks::AIR), tex(Textures::NULLBLOCK), col0(0), col1(0), col2(0), col3(0) {}
	};
	
	std::vector<std::pair<VBOID, GLuint>> RenderChunk(World::Chunk const& c);
	std::vector<std::pair<VBOID, GLuint>> MergeFaceRenderChunk(World::Chunk const& c);
}
