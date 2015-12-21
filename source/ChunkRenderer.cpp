#include "ChunkRenderer.h"

namespace ChunkRenderer {

	void renderPrimitive(int x, int y, int z, int length, int direction, block bl, brightness br){
		double color = (double)br / world::BRIGHTNESSMAX;
		int face = 0;
		if (direction == 2) face = 1;
		else if (direction == 3) face = 3;
		else face = 2;
		renderer::TexCoord3d(0.0, 0.0, (Textures::getTextureIndex(bl, face) - 0.9) / 64.0);
		if (direction == 0) {
			renderer::Color3d(0.7*color, 0.7*color, 0.7*color);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
		}
		if (direction == 1) {
			renderer::Color3d(0.7*color, 0.7*color, 0.7*color);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
		}
		if (direction == 2) {
			renderer::Color3d(1.0*color, 1.0*color, 1.0*color);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x + 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z + length + 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x + 0.5, y + 0.5, z + length + 0.5);
		}
		if (direction == 3) {
			renderer::Color3d(1.0*color, 1.0*color, 1.0*color);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x + 0.5, y - 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x + 0.5, y - 0.5, z + length + 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z + length + 0.5);
		}
		if (direction == 4) {
			renderer::Color3d(0.5*color, 0.5*color, 0.5*color);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z + 0.5);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z + 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x + length + 0.5, y - 0.5, z + 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x + length + 0.5, y + 0.5, z + 0.5);
		}
		if (direction == 5) {
			renderer::Color3d(0.5*color, 0.5*color, 0.5*color);
			renderer::TexCoord2d(0.0, 0.0); renderer::Vertex3d(x - 0.5, y - 0.5, z - 0.5);
			renderer::TexCoord2d(0.0, 1.0); renderer::Vertex3d(x - 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 1.0); renderer::Vertex3d(x + length + 0.5, y + 0.5, z - 0.5);
			renderer::TexCoord2d(length + 1.0, 0.0); renderer::Vertex3d(x + length + 0.5, y - 0.5, z - 0.5);
		}
	}

	void renderChunk(world::chunk* c) {
		int x, y, z;
		renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == blocks::AIR) continue;
					if (!BlockInfo(curr).isTranslucent()) renderblock(x, y, z, c);
				}
			}
		}
		renderer::Flush(c->vbuffer[0], c->vertexes[0]);
		renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == blocks::AIR) continue;
					if (BlockInfo(curr).isTranslucent() && BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		renderer::Flush(c->vbuffer[1], c->vertexes[1]);
		renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == blocks::AIR) continue;
					if (!BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		renderer::Flush(c->vbuffer[2], c->vertexes[2]);
	}

	void mergeFaceRender(world::chunk* c) {
		int cx = c->cx, cy = c->cy, cz = c->cz;
		int x = 0, y = 0, z = 0;
		int cur_x = 0, cur_y = 0, cur_z = 0, cur_l = 0;
		block bl = 0, cur_block = 0, neighbour = 0;
		brightness br = 0, cur_brightness = 0;
		bool valid = false;
		//Linear merge
		renderer::Init(3, 3);
		for (int d = 0; d<6; d++) {
			for (int i = 0; i<16; i++) {
				for (int j = 0; j<16; j++) {
					for (int k = 0; k<16; k++) {
						//Get position
						if (d<2) x = i, y = j, z = k;
						else if (d<4) x = i, y = j, z = k;
						else x = k, y = i, z = j;
						//Get properties
						bl = c->getblock(x, y, z);
						//Get neighbour properties
						int xx = x + delta[d][0], yy = y + delta[d][1], zz = z + delta[d][2];
						int gx = cx * 16 + xx, gy = cy * 16 + yy, gz = cz * 16 + zz;
						if (xx < 0 || xx >= 16 || yy < 0 || yy >= 16 || zz < 0 || zz >= 16) {
							neighbour = world::getblock(gx, gy, gz);
							br = world::getbrightness(gx, gy, gz);
						}
						else {
							neighbour = c->getblock(xx, yy, zz);
							br = c->getbrightness(xx, yy, zz);
						}
						//Render
						if (bl == blocks::AIR || BlockInfo(bl).isTranslucent() || BlockInfo(neighbour).isOpaque()) {
							//Not valid block
							if (valid) {
								renderPrimitive(cur_x, cur_y, cur_z, cur_l, d, cur_block, cur_brightness);
								valid = false;
							}
							continue;
						}
						if (valid) {
							if (bl != cur_block || br != cur_brightness) {
								renderPrimitive(cur_x, cur_y, cur_z, cur_l, d, cur_block, cur_brightness);
								cur_x = x; cur_y = y; cur_z = z; cur_l = 0;
								cur_block = bl; cur_brightness = br;
							}
							else cur_l++;
						}
						else {
							valid = true;
							cur_x = x; cur_y = y; cur_z = z; cur_l = 0;
							cur_block = bl; cur_brightness = br;
						}
					}
					if (valid) {
						renderPrimitive(cur_x, cur_y, cur_z, cur_l, d, cur_block, cur_brightness);
						valid = false;
					}
				}
			}
		}
		renderer::Flush(c->vbuffer[0], c->vertexes[0]);
		renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == blocks::AIR) continue;
					if (BlockInfo(curr).isTranslucent() && BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		renderer::Flush(c->vbuffer[1], c->vertexes[1]);
		renderer::Init(2, 3);
		for (x = 0; x < 16; x++) {
			for (y = 0; y < 16; y++) {
				for (z = 0; z < 16; z++) {
					block curr = c->getblock(x, y, z);
					if (curr == blocks::AIR) continue;
					if (!BlockInfo(curr).isSolid()) renderblock(x, y, z, c);
				}
			}
		}
		renderer::Flush(c->vbuffer[2], c->vertexes[2]);
	}

}