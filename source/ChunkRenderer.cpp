#include "ChunkRenderer.h"
#include "Renderer.h"
#include "World.h"

namespace ChunkRenderer {

	class ChunkRenderData {
	public:
		block* pblocks;
		brightness* pbrightness;

		ChunkRenderData(int cx, int cy, int cz) {
			pblocks = new block[18 * 18 * 18];
			pbrightness = new brightness[18 * 18 * 18];

			World::chunk* chunkptr[3][3][3];

			for (int x = -1; x <= 1; x++)
				for (int y = -1; y <= 1; y++)
					for (int z = -1; z <= 1; z++)
						chunkptr[x + 1][y + 1][z + 1] = World::getChunkPtr(x + cx, y + cy, z + cz);

			for (int x = 0; x < 18; x++) {
				int rcx = 1, bx = x - 1;
				if (x == 0) rcx = 0, bx += 16;
				else if (x == 18 - 1) rcx = 2, bx -= 16;

				for (int y = 0; y < 18; y++) {
					int rcy = 1, by = y - 1;
					if (y == 0) rcy = 0, by += 16;
					else if (y == 18 - 1) rcy = 2, by -= 16;

					for (int z = 0; z < 18; z++) {
						int rcz = 1, bz = z - 1;
						if (z == 0) rcz = 0, bz += 16;
						else if (z == 18 - 1) rcz = 2, bz -= 16;

						if (chunkptr[rcx][rcy][rcz] == nullptr) {
							pblocks[x * 18 * 18 + y * 18 + z] = Blocks::ROCK;
							pbrightness[x * 18 * 18 + y * 18 + z] = World::skylight;
						} else if(chunkptr[rcx][rcy][rcz] == World::EmptyChunkPtr) {
							pblocks[x * 18 * 18 + y * 18 + z] = Blocks::AIR;
							pbrightness[x * 18 * 18 + y * 18 + z] = (rcy - 1 + cy < 0) ? World::BRIGHTNESSMIN : World::skylight;
						} else {
							pblocks[x * 18 * 18 + y * 18 + z] = chunkptr[rcx][rcy][rcz]->getblock(bx, by, bz);
							pbrightness[x * 18 * 18 + y * 18 + z] = chunkptr[rcx][rcy][rcz]->getbrightness(bx, by, bz);
						}
					}
				}
			}
		}

		~ChunkRenderData() {
			delete[] pblocks;
			delete[] pbrightness;
		}

		block getblock(int x, int y, int z) {
			return pblocks[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
		}

		brightness getbrightness(int x, int y, int z) {
			return pbrightness[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
		}
	};

	using World::getbrightness;

	/*
	�ϲ���Ķ���˳����0��3�������

	The vertex order of merge face render
	Numbered from 0 to 3:

	(k++)
	...
	|    |
	+----+--
	|    |
	|    |    |
	3----2----+-
	|curr|    |   ...
	|face|    |   (j++)
	0----1----+--

	--qiaozhanrong
	*/

	void RenderPrimitive(QuadPrimitive& p) {
		float col0 = (float)p.col0 * 0.25f / World::BRIGHTNESSMAX;
		float col1 = (float)p.col1 * 0.25f / World::BRIGHTNESSMAX;
		float col2 = (float)p.col2 * 0.25f / World::BRIGHTNESSMAX;
		float col3 = (float)p.col3 * 0.25f / World::BRIGHTNESSMAX;
		int x = p.x, y = p.y, z = p.z, length = p.length;
#ifdef NERDMODE1
		Renderer::TexCoord3d(0.0, 0.0, (p.tex + 0.5) / 64.0);
		if (p.direction == 0) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(2.0f);
			else col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
		}
		else if (p.direction == 1) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(3.0f);
			else col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
		}
		else if (p.direction == 2) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(4.0f);
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
		}
		else if (p.direction == 3) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(5.0f);
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
		}
		else if (p.direction == 4) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(0.0f);
			else col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
		}
		else if (p.direction == 5) {
			if (Renderer::AdvancedRender) Renderer::Attrib1f(1.0f);
			else col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
			Renderer::Color3f(col0, col0, col0);
			Renderer::TexCoord2d(0.0, 0.0); Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Color3f(col1, col1, col1);
			Renderer::TexCoord2d(0.0, 1.0); Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col2, col2, col2);
			Renderer::TexCoord2d(length + 1.0, 1.0); Renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
			Renderer::Color3f(col3, col3, col3);
			Renderer::TexCoord2d(length + 1.0, 0.0); Renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
		}
#else
		float T3d = (Textures::getTextureIndex(p.block, face) - 0.5) / 64.0;
		switch (p.direction)
		{
		case 0: {
			if (p.block != Blocks::GLOWSTONE) color *= 0.7;
			float geomentry[] = {
				0.0, 0.0, T3d, color, color, color, x + 0.5, y - 0.5, z - 0.5,
				0.0, 1.0, T3d, color, color, color, x + 0.5, y + 0.5, z - 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x + 0.5, y + 0.5, z + length + 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x + 0.5, y - 0.5, z + length + 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		case 1: {
			if (p.block != Blocks::GLOWSTONE) color *= 0.7;
			float geomentry[] = {
				0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
				0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + length + 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + length + 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		case 2: {
			float geomentry[] = {
				0.0, 0.0, T3d, color, color, color, x + 0.5, y + 0.5, z - 0.5,
				0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + length + 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x + 0.5, y + 0.5, z + length + 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		case 3: {
			float geomentry[] = {
				0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
				0.0, 1.0, T3d, color, color, color, x + 0.5, y - 0.5, z - 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x + 0.5, y - 0.5, z + length + 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + length + 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		case 4: {
			if (p.block != Blocks::GLOWSTONE) color *= 0.5;
			float geomentry[] = {
				0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z + 0.5,
				0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z + 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x + length + 0.5, y - 0.5, z + 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x + length + 0.5, y + 0.5, z + 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		case 5: {
			if (p.block != Blocks::GLOWSTONE) color *= 0.5;
			float geomentry[] = {
				0.0, 0.0, T3d, color, color, color, x - 0.5, y - 0.5, z - 0.5,
				0.0, 1.0, T3d, color, color, color, x - 0.5, y + 0.5, z - 0.5,
				length + 1.0, 1.0, T3d, color, color, color, x + length + 0.5, y + 0.5, z - 0.5,
				length + 1.0, 0.0, T3d, color, color, color, x + length + 0.5, y - 0.5, z - 0.5
			};
			Renderer::Quad(geomentry);
		}
				break;
		}
#endif // NERDMODE1

	}

	void RenderPrimitive_Depth(QuadPrimitive_Depth& p) {
		int x = p.x, y = p.y, z = p.z, length = p.length;
		if (p.direction == 0) {
			Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
			Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
		}
		else if (p.direction == 1) {
			Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
			Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
		}
		else if (p.direction == 2) {
			Renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
			Renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
		}
		else if (p.direction == 3) {
			Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			Renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
			Renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
		}
		else if (p.direction == 4) {
			Renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
			Renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
			Renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
			Renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
		}
		else if (p.direction == 5) {
			Renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			Renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
			Renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
		}
	}

	void RenderChunk(World::chunk* c) {
		int x, y, z;
		if (Renderer::AdvancedRender) Renderer::Init(2, 3, 3, 1); else Renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == Blocks::AIR) continue;
					if (!BlockInfo(curr).isTranslucent()) renderblock(x, y, z, c);
				}
			}
		}
		Renderer::Flush(c->vbuffer[0], c->vertexes[0]);
		if (Renderer::AdvancedRender) Renderer::Init(2, 3, 3, 1); else Renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == Blocks::AIR) continue;
					if (BlockInfo(curr).isTranslucent() && BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		Renderer::Flush(c->vbuffer[1], c->vertexes[1]);
		if (Renderer::AdvancedRender) Renderer::Init(2, 3, 3, 1); else Renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == Blocks::AIR) continue;
					if (!BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		Renderer::Flush(c->vbuffer[2], c->vertexes[2]);
	}

	//�ϲ���󷨺ã�����
	void MergeFaceRender(World::chunk* c) {
		//��˵��ע��һ������һ��Ӣ���ǲ����е���֡�����
		// -- qiaozhanrong

		int cx = c->cx, cy = c->cy, cz = c->cz;
		ChunkRenderData rd(cx, cy, cz);
		int x = 0, y = 0, z = 0, cur_l_mx, br;
		int col0 = 0, col1 = 0, col2 = 0, col3 = 0;
		QuadPrimitive cur;
		block bl, neighbour;
		ubyte face = 0;
		TextureID tex;
		bool valid = false;

		for (int steps = 0; steps < 3; steps++) {
			cur = QuadPrimitive();
			cur_l_mx = bl = neighbour = 0;
			//Linear merge
			if (Renderer::AdvancedRender) Renderer::Init(3, 3, 3, 1); else Renderer::Init(3, 3);
			for (int d = 0; d < 6; d++) {
				cur.direction = d;
				if (d == 2) face = 1;
				else if (d == 3) face = 3;
				else face = 2;
				//Render current face
				for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) {
					for (int k = 0; k < 16; k++) {
						//Get position & brightness
						if (d == 0) { //x+
							x = i, y = j, z = k;
							br = rd.getbrightness(x + 1, y, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						else if (d == 1) { //x-
							x = i, y = j, z = k;
							br = rd.getbrightness(x - 1, y, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col2 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						else if (d == 2) { //y+
							x = j, y = i, z = k;
							br = rd.getbrightness(x, y + 1, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						else if (d == 3) { //y-
							x = j, y = i, z = k;
							br = rd.getbrightness(x, y - 1, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						else if (d == 4) { //z+
							x = k, y = j, z = i;
							br = rd.getbrightness(x, y, z + 1);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
								col1 = br + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
								col2 = br + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						else if (d == 5) { //z-
							x = k, y = j, z = i;
							br = rd.getbrightness(x, y, z - 1);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col3 = br + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
						}
						//Get block ID
						bl = rd.getblock(x, y, z);
						tex = Textures::getTextureIndex(bl, face);
						neighbour = rd.getblock(x + delta[d][0], y + delta[d][1], z + delta[d][2]);
						if (NiceGrass && bl == Blocks::GRASS) {
							if (d == 0 && rd.getblock(x + 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							else if (d == 1 && rd.getblock(x - 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							else if (d == 4 && rd.getblock(x, y - 1, z + 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							else if (d == 5 && rd.getblock(x, y - 1, z - 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
						}
						//Render
						const Blocks::SingleBlock& info = BlockInfo(bl);
						if (bl == Blocks::AIR || bl == neighbour && bl != Blocks::LEAF || BlockInfo(neighbour).isOpaque() ||
							steps == 0 && info.isTranslucent() ||
							steps == 1 && (!info.isTranslucent() || !info.isSolid()) ||
							steps == 2 && (!info.isTranslucent() || info.isSolid())) {
							//Not valid block
							if (valid) {
								if (BlockInfo(neighbour).isOpaque() && !cur.once) {
									if (cur_l_mx < cur.length) cur_l_mx = cur.length;
									cur_l_mx++;
								}
								else {
									RenderPrimitive(cur);
									valid = false;
								}
							}
							continue;
						}
						if (valid) {
							if (col0 != col1 || col1 != col2 || col2 != col3 || cur.once || tex != cur.tex || col0 != cur.col0) {
								RenderPrimitive(cur);
								cur.x = x; cur.y = y; cur.z = z; cur.length = cur_l_mx = 0;
								cur.tex = tex; cur.col0 = col0; cur.col1 = col1; cur.col2 = col2; cur.col3 = col3;
								if (col0 != col1 || col1 != col2 || col2 != col3) cur.once = true; else cur.once = false;
							}
							else {
								if (cur_l_mx > cur.length) cur.length = cur_l_mx;
								cur.length++;
							}
						}
						else {
							valid = true;
							cur.x = x; cur.y = y; cur.z = z; cur.length = cur_l_mx = 0;
							cur.tex = tex; cur.col0 = col0; cur.col1 = col1; cur.col2 = col2; cur.col3 = col3;
							if (col0 != col1 || col1 != col2 || col2 != col3) cur.once = true; else cur.once = false;
						}
					}
					if (valid) {
						RenderPrimitive(cur);
						valid = false;
					}
				}
			}
			Renderer::Flush(c->vbuffer[steps], c->vertexes[steps]);
		}
	}

	void RenderDepthModel(World::chunk* c) {
		int cx = c->cx, cy = c->cy, cz = c->cz;
		int x = 0, y = 0, z = 0;
		QuadPrimitive_Depth cur;
		int cur_l_mx;
		block bl, neighbour;
		bool valid = false;
		cur_l_mx = bl = neighbour = 0;
		//Linear merge for depth model
		Renderer::Init(0, 0);
		for (int d = 0; d < 6; d++) {
			cur.direction = d;
			for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) {
				for (int k = 0; k < 16; k++) {
					//Get position
					if (d < 2) x = i, y = j, z = k;
					else if (d < 4) x = i, y = j, z = k;
					else x = k, y = i, z = j;
					//Get block ID
					bl = c->getblock(x, y, z);
					//Get neighbour ID
					int xx = x + delta[d][0], yy = y + delta[d][1], zz = z + delta[d][2];
					int gx = cx * 16 + xx, gy = cy * 16 + yy, gz = cz * 16 + zz;
					if (xx < 0 || xx >= 16 || yy < 0 || yy >= 16 || zz < 0 || zz >= 16)
						neighbour = World::getblock(gx, gy, gz);
					else neighbour = c->getblock(xx, yy, zz);
					//Render
					if (bl == Blocks::AIR || bl == Blocks::GLASS || bl == neighbour && bl != Blocks::LEAF ||
						BlockInfo(neighbour).isOpaque() || BlockInfo(bl).isTranslucent()) {
						//Not valid block
						if (valid) {
							if (BlockInfo(neighbour).isOpaque()) {
								if (cur_l_mx < cur.length) cur_l_mx = cur.length;
								cur_l_mx++;
							}
							else {
								RenderPrimitive_Depth(cur);
								valid = false;
							}
						}
						continue;
					}
					if (valid) {
						if (cur_l_mx > cur.length) cur.length = cur_l_mx;
						cur.length++;
					}
					else {
						valid = true;
						cur.x = x; cur.y = y; cur.z = z; cur.length = cur_l_mx = 0;
					}
				}
				if (valid) {
					RenderPrimitive_Depth(cur);
					valid = false;
				}
			}
		}
		Renderer::Flush(c->vbuffer[3], c->vertexes[3]);
	}
}