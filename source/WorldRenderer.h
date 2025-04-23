#pragma once
#include "StdInclude.h"
#include "World.h"
#include "Renderer.h"
#include "FrustumTest.h"

namespace WorldRenderer {
	enum {
		MainShader, MergeFaceShader, ShadowShader, DepthShader, ShowDepthShader
	};

	struct RenderChunk {
		int cx, cy, cz;
		std::array<std::pair<VBOID, GLuint>, 2> meshes;
		float loadAnim;
		RenderChunk(World::Chunk* c, float TimeDelta) :
			cx(c->x()), cy(c->y()), cz(c->z()), loadAnim(c->loadAnimOffset() * std::pow(0.6f, TimeDelta)) {
			meshes[0] = c->mesh(0);
			meshes[1] = c->mesh(1);
		}
	};
	extern vector<RenderChunk> RenderChunkList;

	void ListRenderChunks(double x, double y, double z, int renderdistance, double curtime, std::optional<FrustumTest> frustum);
	void RenderChunks(double x, double y, double z, int buffer);
}
