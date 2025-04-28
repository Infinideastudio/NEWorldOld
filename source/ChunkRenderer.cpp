#include "ChunkRenderer.h"
#include "Renderer.h"
#include "World.h"

namespace ChunkRenderer {

	class ChunkRenderData {
	public:
		std::vector<BlockID> pblocks;
		std::vector<Brightness> pbrightness;

		ChunkRenderData(World::Chunk const& c) : pblocks(18 * 18 * 18), pbrightness(18 * 18 * 18) {
			int cx = c.x(), cy = c.y(), cz = c.z();
			World::Chunk* chunkptr[3][3][3];

			for (int x = -1; x <= 1; x++)
				for (int y = -1; y <= 1; y++)
					for (int z = -1; z <= 1; z++)
						chunkptr[x + 1][y + 1][z + 1] = World::getChunkPtr(x + cx, y + cy, z + cz);

			int index = 0;
			for (int x = -1; x < 17; x++) {
				int rcx = 0, bx = x;
				if (x < 0) rcx--, bx += 16;
				else if (x >= 16) rcx++, bx -= 16;

				for (int y = -1; y < 17; y++) {
					int rcy = 0, by = y;
					if (y < 0) rcy--, by += 16;
					else if (y >= 16) rcy++, by -= 16;

					for (int z = -1; z < 17; z++) {
						int rcz = 0, bz = z;
						if (z < 0) rcz--, bz += 16;
						else if (z >= 16) rcz++, bz -= 16;

						World::Chunk* p = chunkptr[rcx + 1][rcy + 1][rcz + 1];
						if (p == nullptr) {
							pblocks[index] = Blocks::ROCK;
							pbrightness[index] = World::skylight;
						} else if(p == World::EmptyChunkPtr) {
							pblocks[index] = Blocks::AIR;
							pbrightness[index] = (cy + rcy < 0) ? World::BRIGHTNESSMIN : World::skylight;
						} else {
							pblocks[index] = p->getblock(bx, by, bz);
							pbrightness[index] = p->getbrightness(bx, by, bz);
						}
						index++;
					}
				}
			}
		}

		BlockID getblock(int x, int y, int z) {
			return pblocks[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
		}

		Brightness getbrightness(int x, int y, int z) {
			return pbrightness[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
		}
	};

	void RenderBlock(int x, int y, int z, ChunkRenderData& rd) {
		float col1, col2, col3, col4;

		BlockID bl = rd.getblock(x, y, z);
		BlockID neighbors[6] = {
			rd.getblock(x + 1, y, z),
			rd.getblock(x - 1, y, z),
			rd.getblock(x, y + 1, z),
			rd.getblock(x, y - 1, z),
			rd.getblock(x, y, z + 1),
			rd.getblock(x, y, z - 1),
		};
		TextureIndex tex;

		// Right face
		if (!(bl == Blocks::AIR || bl == neighbors[0] && bl != Blocks::LEAF || BlockInfo(neighbors[0]).isOpaque())) {
			if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x + 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
			else tex = Textures::getTextureIndex(bl, 2);
			col1 = col2 = col3 = col4 = rd.getbrightness(x + 1, y, z);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			if (!Renderer::AdvancedRender) col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(1.0f, 0.0f, 0.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
		}

		// Left Face
		if (!(bl == Blocks::AIR || bl == neighbors[1] && bl != Blocks::LEAF || BlockInfo(neighbors[1]).isOpaque())) {
			if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x - 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
			else tex = Textures::getTextureIndex(bl, 2);
			col1 = col2 = col3 = col4 = rd.getbrightness(x - 1, y, z);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			if (!Renderer::AdvancedRender) col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
		}

		// Top Face
		if (!(bl == Blocks::AIR || bl == neighbors[2] && bl != Blocks::LEAF || BlockInfo(neighbors[2]).isOpaque())) {
			tex = Textures::getTextureIndex(bl, 1);
			col1 = col2 = col3 = col4 = rd.getbrightness(x, y + 1, z);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y + 1, z - 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y + 1, z + 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y + 1, z + 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y + 1, z - 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(0.0f, 1.0f, 0.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
		}

		// Bottom Face
		if (!(bl == Blocks::AIR || bl == neighbors[3] && bl != Blocks::LEAF || BlockInfo(neighbors[3]).isOpaque())) {
			tex = Textures::getTextureIndex(bl, 3);
			col1 = col2 = col3 = col4 = rd.getbrightness(x, y - 1, z);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y - 1, z - 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y - 1, z - 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y - 1, z + 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y - 1, z + 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(0.0f, -1.0f, 0.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
		}

		// Front Face
		if (!(bl == Blocks::AIR || bl == neighbors[4] && bl != Blocks::LEAF || BlockInfo(neighbors[4]).isOpaque())) {
			if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x, y - 1, z + 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
			else tex = Textures::getTextureIndex(bl, 2);
			col1 = col2 = col3 = col4 = rd.getbrightness(x, y, z + 1);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			if (!Renderer::AdvancedRender) col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(0.0f, 0.0f, 1.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
		}

		// Back Face
		if (!(bl == Blocks::AIR || bl == neighbors[5] && bl != Blocks::LEAF || BlockInfo(neighbors[5]).isOpaque())) {
			if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x, y - 1, z - 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
			else tex = Textures::getTextureIndex(bl, 2);
			col1 = col2 = col3 = col4 = rd.getbrightness(x, y, z - 1);
			if (SmoothLighting) {
				col1 = (col1 + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1)) / 4.0f;
				col2 = (col2 + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1)) / 4.0f;
				col3 = (col3 + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1)) / 4.0f;
				col4 = (col4 + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1)) / 4.0f;
			}
			col1 /= World::BRIGHTNESSMAX, col2 /= World::BRIGHTNESSMAX, col3 /= World::BRIGHTNESSMAX, col4 /= World::BRIGHTNESSMAX;
			if (!Renderer::AdvancedRender) col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
			Renderer::Attrib1f(static_cast<float>(bl));
			Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
			Renderer::Normal3f(0.0f, 0.0f, -1.0f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
			Renderer::Color3f(col4, col4, col4); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
		}
	}

	/*
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
	*/

	void RenderPrimitive(QuadPrimitive& p) {
		float col0 = (float)p.col0 * 0.25f / World::BRIGHTNESSMAX;
		float col1 = (float)p.col1 * 0.25f / World::BRIGHTNESSMAX;
		float col2 = (float)p.col2 * 0.25f / World::BRIGHTNESSMAX;
		float col3 = (float)p.col3 * 0.25f / World::BRIGHTNESSMAX;
		int x = p.x, y = p.y, z = p.z, length = p.length;

		Renderer::Attrib1f(static_cast<float>(p.blk));
		Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(p.tex));

		switch (p.direction) {
		case 0:
			if (!Renderer::AdvancedRender) col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
			Renderer::Normal3f(1.0f, 0.0f, 0.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
			break;
		case 1:
			if (!Renderer::AdvancedRender) col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
			Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
			break;
		case 2:
			Renderer::Normal3f(0.0f, 1.0f, 0.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
			break;
		case 3:
			Renderer::Normal3f(0.0f, -1.0f, 0.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
			break;
		case 4:
			if (!Renderer::AdvancedRender) col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
			Renderer::Normal3f(0.0f, 0.0f, 1.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x + length + 0.5f, y - 0.5f, z + 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x + length + 0.5f, y + 0.5f, z + 0.5f);
			break;
		case 5:
			if (!Renderer::AdvancedRender) col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
			Renderer::Normal3f(0.0f, 0.0f, -1.0f);
			Renderer::Color3f(col0, col0, col0); Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::Color3f(col1, col1, col1); Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col2, col2, col2); Renderer::TexCoord2f(length + 1.0f, 1.0f); Renderer::Vertex3f(x + length + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::Color3f(col3, col3, col3); Renderer::TexCoord2f(length + 1.0f, 0.0f); Renderer::Vertex3f(x + length + 0.5f, y - 0.5f, z - 0.5f);
			break;
		}
	}

	std::vector<Renderer::VertexBuffer> RenderChunk(World::Chunk const& c) {
		std::vector<Renderer::VertexBuffer> res;
		ChunkRenderData rd(c);

		for (int steps = 0; steps < 2; steps++) {
			if (Renderer::AdvancedRender) Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
			else Renderer::Begin(GL_QUADS, 3, 3, 1);
			for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++) for (int z = 0; z < 16; z++) {
				const Blocks::SingleBlock& info = BlockInfo(rd.getblock(x, y, z));
				if (steps == 0 && !info.isTranslucent() || steps == 1 && info.isTranslucent()) {
					RenderBlock(x, y, z, rd);
				}
			}
			res.emplace_back(Renderer::End(true));
		}
		return res;
	}

	std::vector<Renderer::VertexBuffer> MergeFaceRenderChunk(World::Chunk const& c) {
		std::vector<Renderer::VertexBuffer> res;
		ChunkRenderData rd(c);

		for (int steps = 0; steps < 2; steps++) {
			if (Renderer::AdvancedRender) Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
			else Renderer::Begin(GL_QUADS, 3, 3, 1);
			for (int d = 0; d < 6; d++) {
				uint8_t face = 0;
				if (d == 2) face = 1;
				else if (d == 3) face = 3;
				else face = 2;
				// Render current face
				for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) {
					QuadPrimitive cur;
					bool valid = false;
					// Linear merge
					for (int k = 0; k < 16; k++) {
						// Get position
						int x = 0, y = 0, z = 0;
						int dx = 0, dy = 0, dz = 0;
						switch (d) {
						case 0: x = i, y = j, z = k, dx = 1; break;
						case 1: x = i, y = j, z = k, dx = -1; break;
						case 2: x = j, y = i, z = k, dy = 1; break;
						case 3: x = j, y = i, z = k, dy = -1; break;
						case 4: x = k, y = j, z = i, dz = 1; break;
						case 5: x = k, y = j, z = i, dz = -1; break;
						}
						// Get block ID
						BlockID bl = rd.getblock(x, y, z);
						BlockID neighbour = rd.getblock(x + dx, y + dy, z + dz);
						const Blocks::SingleBlock& info = BlockInfo(bl);
						if (bl == Blocks::AIR || bl == neighbour && bl != Blocks::LEAF || BlockInfo(neighbour).isOpaque() ||
							!(steps == 0 && !info.isTranslucent() || steps == 1 && info.isTranslucent())) {
							// Not valid block
							if (valid) {
								RenderPrimitive(cur);
								valid = false;
							}
							continue;
						}
						// Get texture and brightness
						TextureIndex tex = Textures::getTextureIndex(bl, face);
						int br = 0, col0 = 0, col1 = 0, col2 = 0, col3 = 0;
						switch (d) {
						case 0:
							if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x + 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							br = rd.getbrightness(x + 1, y, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						case 1:
							if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x - 1, y - 1, z) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							br = rd.getbrightness(x - 1, y, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col2 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						case 2:
							br = rd.getbrightness(x, y + 1, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x - 1, y + 1, z) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y + 1, z) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						case 3:
							br = rd.getbrightness(x, y - 1, z);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y - 1, z) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x - 1, y - 1, z) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						case 4:
							if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x, y - 1, z + 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							br = rd.getbrightness(x, y, z + 1);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x - 1, y + 1, z + 1);
								col1 = br + rd.getbrightness(x - 1, y, z + 1) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x - 1, y - 1, z + 1);
								col2 = br + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x, y - 1, z + 1) + rd.getbrightness(x + 1, y - 1, z + 1);
								col3 = br + rd.getbrightness(x + 1, y, z + 1) + rd.getbrightness(x, y + 1, z + 1) + rd.getbrightness(x + 1, y + 1, z + 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						case 5:
							if (NiceGrass && bl == Blocks::GRASS && rd.getblock(x, y - 1, z - 1) == Blocks::GRASS) tex = Textures::getTextureIndex(bl, 1);
							br = rd.getbrightness(x, y, z - 1);
							if (SmoothLighting) {
								col0 = br + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x - 1, y - 1, z - 1);
								col1 = br + rd.getbrightness(x - 1, y, z - 1) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x - 1, y + 1, z - 1);
								col2 = br + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x, y + 1, z - 1) + rd.getbrightness(x + 1, y + 1, z - 1);
								col3 = br + rd.getbrightness(x + 1, y, z - 1) + rd.getbrightness(x, y - 1, z - 1) + rd.getbrightness(x + 1, y - 1, z - 1);
							}
							else col0 = col1 = col2 = col3 = br * 4;
							break;
						}
						// Render
						bool once = col0 != col1 || col1 != col2 || col2 != col3;
						if (valid) {
							if (once || cur.once || bl != cur.blk || tex != cur.tex || col0 != cur.col0) {
								RenderPrimitive(cur);
								cur.x = x; cur.y = y; cur.z = z; cur.length = 0; cur.direction = d;
								cur.once = once; cur.blk = bl; cur.tex = tex; cur.col0 = col0; cur.col1 = col1; cur.col2 = col2; cur.col3 = col3;
							}
							else cur.length++;
						}
						else {
							valid = true;
							cur.x = x; cur.y = y; cur.z = z; cur.length = 0; cur.direction = d;
							cur.once = once; cur.blk = bl; cur.tex = tex; cur.col0 = col0; cur.col1 = col1; cur.col2 = col2; cur.col3 = col3;
						}
					}
					if (valid) {
						RenderPrimitive(cur);
						valid = false;
					}
				}
			}
			res.emplace_back(Renderer::End(true));
		}
		return res;
	}
}
