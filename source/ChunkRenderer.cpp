#include "Definitions.h"
#include "ChunkRenderer.h"
#include "Renderer.h"
#include "World.h"
#include "Textures.h"

namespace ChunkRenderer {

	void renderPrimitive(QuadPrimitive& p){
		float color = (float)p.brightness / World::BRIGHTNESSMAX;
		int x = p.x, y = p.y, z = p.z, length = p.length;
		ubyte face = 0;
		if (p.direction == 2) face = 1;
		else if (p.direction == 3) face = 3;
		else face = 2;
#ifdef NERDMODE1
		Renderer::TexCoord3d(0.0f, 0.0f, (Textures::getTextureIndex(p.block, face) - 0.5f) / 64.0);
		if (p.direction == 0) {
			if (p.block != Blocks::GLOWSTONE) color *= 0.7;
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x + 0.5f, y - 0.5f, z - 0.5f);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x + 0.5f, y + 0.5f, z + length + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x + 0.5f, y - 0.5f, z + length + 0.5f);
		}
		else if (p.direction == 1) {
			if (p.block != Blocks::GLOWSTONE) color *= 0.7;
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z + length + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z + length + 0.5f);
		}
		else if (p.direction == 2) {
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z + length + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x + 0.5f, y + 0.5f, z + length + 0.5f);
		}
		else if (p.direction == 3) {
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x + 0.5f, y - 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x + 0.5f, y - 0.5f, z + length + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z + length + 0.5f);
		}
		else if (p.direction == 4) {
			if (p.block != Blocks::GLOWSTONE) color *= 0.5f;
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z + 0.5f);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x + length + 0.5f, y - 0.5f, z + 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x + length + 0.5f, y + 0.5f, z + 0.5f);
		}
		else if (p.direction == 5) {
			if (p.block != Blocks::GLOWSTONE) color *= 0.5f;
			Renderer::Color3d(color, color, color);
			Renderer::TexCoord2d(0.0f, 0.0f); Renderer::Vertex3d(x - 0.5f, y - 0.5f, z - 0.5f);
			Renderer::TexCoord2d(0.0f, 1.0f); Renderer::Vertex3d(x - 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 1.0f); Renderer::Vertex3d(x + length + 0.5f, y + 0.5f, z - 0.5f);
			Renderer::TexCoord2d(length + 1.0f, 0.0f); Renderer::Vertex3d(x + length + 0.5f, y - 0.5f, z - 0.5f);
		}
#else
		float T3d = (Textures::getTextureIndex(p.block, face) - 0.5f) / 64.0;
		switch (p.direction)
		{
			case 0: {
				if (p.block != Blocks::GLOWSTONE) color *= 0.7f;
				float geomentry[] = {
					0.0f, 0.0f, T3d, color, color, color, x + 0.5f, y - 0.5f, z - 0.5f,
					0.0f, 1.0f, T3d, color, color, color, x + 0.5f, y + 0.5f, z - 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x + 0.5f, y + 0.5f, z + length + 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x + 0.5f, y - 0.5f, z + length + 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
			case 1: {
				if (p.block != Blocks::GLOWSTONE) color *= 0.7f;
				float geomentry[] = {
					0.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z - 0.5f,
					0.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z - 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z + length + 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z + length + 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
			case 2: {
				float geomentry[] = {
					0.0f, 0.0f, T3d, color, color, color, x + 0.5f, y + 0.5f, z - 0.5f,
					0.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z - 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z + length + 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x + 0.5f, y + 0.5f, z + length + 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
			case 3: {
				float geomentry[] = {
					0.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z - 0.5f,
					0.0f, 1.0f, T3d, color, color, color, x + 0.5f, y - 0.5f, z - 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x + 0.5f, y - 0.5f, z + length + 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z + length + 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
			case 4: {
				if (p.block != Blocks::GLOWSTONE) color *= 0.5f;
				float geomentry[] = {
					0.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z + 0.5f,
					0.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z + 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x + length + 0.5f, y - 0.5f, z + 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x + length + 0.5f, y + 0.5f, z + 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
			case 5: {
				if (p.block != Blocks::GLOWSTONE) color *= 0.5f;
				float geomentry[] = {
					0.0f, 0.0f, T3d, color, color, color, x - 0.5f, y - 0.5f, z - 0.5f,
					0.0f, 1.0f, T3d, color, color, color, x - 0.5f, y + 0.5f, z - 0.5f,
					length + 1.0f, 1.0f, T3d, color, color, color, x + length + 0.5f, y + 0.5f, z - 0.5f,
					length + 1.0f, 0.0f, T3d, color, color, color, x + length + 0.5f, y - 0.5f, z - 0.5f
				};
				Renderer::Quad(geomentry);
			}
			break;
		}
#endif // NERDMODE1
				
	}

	void renderChunk(World::chunk* c) {
		int x, y, z;
		Renderer::Init(2, 3);
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
		Renderer::Init(2, 3);
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
		Renderer::Init(2, 3);
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

	void mergeFaceRender(World::chunk* c) {
		int cx = c->cx, cy = c->cy, cz = c->cz;
		int x = 0, y = 0, z = 0;
		QuadPrimitive cur;
		int cur_l_mx;
		block bl, neighbour;
		brightness br;
		bool valid = false;
		for (int steps = 0; steps < 3; steps++) {
			cur = QuadPrimitive();
			cur_l_mx = bl = neighbour = br = 0;
			//Linear merge
			Renderer::Init(3, 3);
			for (int d = 0; d < 6; d++){
				cur.direction = d;
				for (int i = 0; i < 16; i++) for (int j = 0; j < 16; j++) {
					for (int k = 0; k < 16; k++) {
						//Get position
						if (d < 2) x = i, y = j, z = k;
						else if (d < 4) x = i, y = j, z = k;
						else x = k, y = i, z = j;
						//Get properties
						bl = c->getblock(x, y, z);
						//Get neighbour properties
						int xx = x + delta[d][0], yy = y + delta[d][1], zz = z + delta[d][2];
						int gx = cx * 16 + xx, gy = cy * 16 + yy, gz = cz * 16 + zz;
						if (xx < 0 || xx >= 16 || yy < 0 || yy >= 16 || zz < 0 || zz >= 16) {
							neighbour = World::getblock(gx, gy, gz);
							br = World::getbrightness(gx, gy, gz);
						}
						else {
							neighbour = c->getblock(xx, yy, zz);
							br = c->getbrightness(xx, yy, zz);
						}
						//Render
						const Blocks::SingleBlock& info = BlockInfo(bl);
						if (bl == Blocks::AIR || bl == neighbour && bl != Blocks::LEAF || BlockInfo(neighbour).isOpaque() ||
							steps == 0 && info.isTranslucent() ||
							steps == 1 && (!info.isTranslucent() || !info.isSolid()) ||
							steps == 2 && (!info.isTranslucent() || info.isSolid())) {
							//Not valid block
							if (valid) {
								if (BlockInfo(neighbour).isOpaque()) {
									if (cur_l_mx < cur.length) cur_l_mx = cur.length;
									cur_l_mx++;
								}
								else {
									renderPrimitive(cur);
									valid = false;
								}
							}
							continue;
						}
						if (valid) {
							if (bl != cur.block || br != cur.brightness) {
								renderPrimitive(cur);
								cur.x = x; cur.y = y; cur.z = z; cur.length = cur_l_mx = 0;
								cur.block = bl; cur.brightness = br;
							}
							else {
								if (cur_l_mx > cur.length) cur.length = cur_l_mx;
								cur.length++;
							}
						}
						else {
							valid = true;
							cur.x = x; cur.y = y; cur.z = z; cur.length = cur_l_mx = 0;
							cur.block = bl; cur.brightness = br;
						}
					}
					if (valid) {
						renderPrimitive(cur);
						valid = false;
					}
				}
			}
			Renderer::Flush(c->vbuffer[steps], c->vertexes[steps]);
		}
	}

}