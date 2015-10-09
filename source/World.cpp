#include "World.h"
#include "Textures.h"
#include "Renderer.h"
#include "WorldGen.h"

extern bool UseCIArray;
extern int viewdistance;

namespace world{
	
	string worldname;
	brightness skylight = 15;         //Sky light level
	brightness BRIGHTNESSMAX = 15;    //Maximum brightness
	brightness BRIGHTNESSMIN = 2;     //Mimimum brightness
	brightness BRIGHTNESSDEC = 1;    //Brightness decrease
	chunk EmptyChunk = chunk();
	unsigned int EmptyBuffer;

	chunk* chunks;
	int loadedChunks, chunkArraySize;
	int ciCacheIndex = -1;
	unsigned long long ciCacheID = 0;
	bool ciArrayAval;
	chunkIndexArray ciArray;
	HeightMap HMap;
	int cloud[128][128];
	int rebuiltChunks, rebuiltChunksCount;
	int updatedChunks, updatedChunksCount;
	int unloadedChunks, unloadedChunksCount;
	int chunkBuildRenderList[256][2];
	int chunkLoadList[256][4];
	int chunkUnloadList[256][4];
	vector<unsigned int> vbuffersShouldDelete;
	int chunkBuildRenders, chunkLoads, chunkUnloads;
	bool* loadedChunkArray = nullptr; //Accelerate sort

	void Init(){
		
		std::stringstream ss;
		system("mkdir Worlds");
		ss << "mkdir Worlds\\" << worldname;
		system(ss.str().c_str());
		ss.clear(); ss.str("");
		ss << "mkdir Worlds\\" << worldname << "\\chunks";
		system(ss.str().c_str());

		WorldGen::perlinNoiseInit(3404);
		ciCacheIndex = -1;
		ciCacheID = 0;

		if (UseCIArray){
			ciArray.setSize((viewdistance + 2) * 2);
			ciArrayAval = !ciArray.create();
			if (!ciArrayAval)DebugWarning("ChunkIndexArray not avaliable because it couldn't be created.");
		}
		else ciArrayAval = false;
		HMap.setSize((viewdistance + 2) * 2 * 16);
		HMap.create();
		loadedChunkArray = nullptr;

	}

	chunk* AddChunk(int x, int y, int z){
		int first, last, middle, i;
		unsigned long long cid;
		cid = getChunkID(x, y, z);                                                   //Chunk ID
		//二分查找,GO!
		first = 0;
		last = loadedChunks - 1;
		middle = (first + last) / 2;
		if (loadedChunks > 0){
			while (first <= last && chunks[middle].id != cid){
				if (chunks[middle].id > cid) last = middle - 1;
				if (chunks[middle].id < cid) first = middle + 1;
				middle = (first + last) / 2;
			}
			if (chunks[middle].id == cid){
				printf("[Console][Error]");
				printf("Chunk(%d,%d,%d)has been loaded,when adding chunk.\n", x, y, z);
				return &chunks[middle];
			}
		}
		ExpandChunkArray(1);
		for (i = loadedChunks - 1; i >= first + 1; i--){
			chunks[i] = chunks[i - 1];
		}
		chunks[first] = EmptyChunk;
		chunks[first].Init(x, y, z, cid);
		if (ciCacheIndex>=first)ciCacheIndex++;
		if(ciArrayAval)ciArray.AddChunk(first,x,y,z);
		return &chunks[first];
	}

	void DeleteChunk(int index){
		for (int i = index; i < loadedChunks - 1; i++){
			chunks[i] = chunks[i + 1];
		}
		chunks[loadedChunks - 1] = EmptyChunk;
		ReduceChunkArray(1);
		if (ciCacheIndex == index){
			ciCacheID = 0;
			ciCacheIndex = -1;
		}
		if (ciCacheIndex > index)ciCacheIndex--;
		if(ciArrayAval)ciArray.DeleteChunk(index);
	}

	uint64 getChunkID(int x, int y, int z){
		//x = -134217728 ~ 134217727      (28 bits)
		//y = -128       ~ 127            (8 bits)
		//z = -134217728 ~ 134217727      (28 bits)
		if (y == -128) y = 0;
		if (x == -134217728) x = 0;
		if (z == -134217728) z = 0;
		if (y <= 0) y = abs(y) + (1LL << 7);
		if (x <= 0) x = abs(x) + (1LL << 27);
		if (z <= 0) z = abs(z) + (1LL << 27);
		return (uint64(y) << 56) + (uint64(x) << 28) + z;
	}

	bool chunkLoaded(int x, int y, int z){
		if (chunkOutOfBound(x, y, z))return false;
		if (getChunkIndex(x, y, z) != -1)return true;
		return false;
	}

	int getChunkIndex(int x, int y, int z){
		unsigned long long cid = getChunkID(x, y, z);
		int ret;
		if (ciCacheID == cid && ciCacheIndex != -1){
			return ciCacheIndex;
		}
		else{
			if (ciArrayAval){
				ret = ciArray.getChunkIndex(x, y, z);
				if (ret != -1){
					ciCacheID = cid;
					ciCacheIndex = ret;
					return ret;
				}
			}
			int first, last, middle;                                        //Chunk ID
			ret = -1;
			//二分查找,GO!
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
			c_getChunkIndexFromSearch++;
#endif
			first = 0;
			last = loadedChunks - 1;
			middle = (first + last) / 2;
			if (loadedChunks > 0){
				while (first <= last && chunks[middle].id != cid){
					if (chunks[middle].id > cid) last = middle - 1;
					if (chunks[middle].id < cid) first = middle + 1;
					middle = (first + last) / 2;
				}
				if (chunks[middle].id == cid){
					ret = middle;
					ciCacheID = cid;
					ciCacheIndex = middle;
					if (ciArrayAval && ciArray.elementExists(x - ciArray.originX, y - ciArray.originY, z - ciArray.originZ)){
						ciArray.array[(x - ciArray.originX)*ciArray.size2 + (y - ciArray.originY)*ciArray.size + (z - ciArray.originZ)] = middle;
					}
				}
			}
		}
		return ret;

	}

	chunk* getChunkPtr(int x, int y, int z){
		
		int i = getChunkIndex(x, y, z);
		if (i != -1)return &chunks[i];
		printf("[Console][Error]");
		printf("Chunk(%d,%d,%d)is not exist,when getting pointer.\n", x, y, z);
		return nullptr;

	}

	void ExpandChunkArray(int cc){

		loadedChunks += cc;
		if (loadedChunks > chunkArraySize) {
			chunkArraySize += 1024;
			chunks = (chunk*)realloc(chunks, chunkArraySize * sizeof(chunk));
			if (chunks == nullptr && loadedChunks != 0) {
				printf("[Console][Error]");
				printf("Chunk Array expanding error.\n");
				saveAllChunks();
				destroyAllChunks();
				glfwTerminate();
				exit(0);
			}
		}

	}

	void ReduceChunkArray(int cc){

		loadedChunks -= cc;
		if (chunkArraySize - 1024 > loadedChunks){
			chunkArraySize -= 1024;
			chunks = (chunk*)realloc(chunks, chunkArraySize * sizeof(chunk));
			if (chunks == nullptr && loadedChunks != 0) {
				printf("[Console][Error]");
				printf("Chunk Array reducing error.\n");
				saveAllChunks();
				destroyAllChunks();
				glfwTerminate();
				exit(0);
			}
		}

	}

	void renderblock(int x, int y, int z, int chunkindex) {
		
		chunk* chunkptr = &chunks[chunkindex];
		double colors, color1, color2, color3, color4, tcx, tcy, size, EPS = 0.0;
		int cx = chunkptr->cx, cy = chunkptr->cy, cz = chunkptr->cz;
		int gx = cx * 16 + x, gy = cy * 16 + y, gz = cz * 16 + z;
		block blk[7] = { chunkptr->getblock(x,y,z) ,
			z < 15 ? chunkptr->getblock(x, y, z + 1) : getblock(gx, gy, gz + 1, blocks::ROCK),
			z>0 ? chunkptr->getblock(x, y, z - 1) : getblock(gx, gy, gz - 1, blocks::ROCK),
			x < 15 ? chunkptr->getblock(x + 1, y, z) : getblock(gx + 1, gy, gz, blocks::ROCK),
			x>0 ? chunkptr->getblock(x - 1, y, z) : getblock(gx - 1, gy, gz, blocks::ROCK),
			y < 15 ? chunkptr->getblock(x, y + 1, z) : getblock(gx, gy + 1, gz, blocks::ROCK),
			y>0 ? chunkptr->getblock(x, y - 1, z) : getblock(gx, gy - 1, gz, blocks::ROCK) };

		brightness brt[7] = { chunkptr->getbrightness(x,y,z) ,
			z < 15 ? chunkptr->getbrightness(x,y,z + 1) : getbrightness(gx,gy,gz + 1),
			z>0 ? chunkptr->getbrightness(x,y,z - 1) : getbrightness(gx,gy,gz - 1),
			x < 15 ? chunkptr->getbrightness(x + 1,y,z) : getbrightness(gx + 1,gy,gz),
			x>0 ? chunkptr->getbrightness(x - 1,y,z) : getbrightness(gx - 1,gy,gz),
			y < 15 ? chunkptr->getbrightness(x,y + 1,z) : getbrightness(gx,gy + 1,gz),
			y>0 ? chunkptr->getbrightness(x,y - 1,z) : getbrightness(gx,gy - 1,gz) };

		size = 1 / 8.0f - EPS;
		
		if (blk[0] == blocks::GRASS && getblock(gx, gy - 1, gz + 1, blocks::ROCK, chunkindex) == blocks::GRASS) {
			tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
		}
		else {
			tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
		}

		// Front Face
		if (!(BlockInfo(blk[1]).isOpaque() || (blk[1] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF) {

			colors = brt[1];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
				color2 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
				color3 = (colors + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
				color4 = (colors + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;
			if (blk[0] != blocks::GLOWSTONE){
				color1 *= 0.5;
				color2 *= 0.5;
				color3 *= 0.5;
				color4 *= 0.5;
			}

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx, tcy); renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size, tcy); renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size, tcy + size); renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx, tcy + size); renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);

		}

		if (blk[0] == blocks::GRASS && getblock(gx, gy - 1, gz - 1, blocks::ROCK, chunkindex) == blocks::GRASS) {
			tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
		}
		else {
			tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
		}

		// Back Face
		if (!(BlockInfo(blk[2]).isOpaque() || (blk[2] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF) {

			colors = brt[2];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
				color2 = (colors + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
				color3 = (colors + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
				color4 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;
			if (blk[0] != blocks::GLOWSTONE){
				color1 *= 0.5;
				color2 *= 0.5;
				color3 *= 0.5;
				color4 *= 0.5;
			}

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*0.0); renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*1.0); renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*1.0); renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*0.0); renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);

		}

		if (blk[0] == blocks::GRASS && getblock(gx + 1, gy - 1, gz, blocks::ROCK, chunkindex) == blocks::GRASS) {
			tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
		}
		else {
			tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
		}
		// Right face
		if (!(BlockInfo(blk[3]).isOpaque() || (blk[3] == blk[0] && !BlockInfo(blk[0]).isOpaque())) || blk[0] == blocks::LEAF) {

			colors = brt[3];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (colors + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
				color2 = (colors + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy, gz - 1) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
				color3 = (colors + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
				color4 = (colors + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy, gz + 1) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;
			if (blk[0] != blocks::GLOWSTONE){
				color1 *= 0.7;
				color2 *= 0.7;
				color3 *= 0.7;
				color4 *= 0.7;
			}

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*0.0); renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*1.0); renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*1.0); renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*0.0); renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);

		}

		if (blk[0] == blocks::GRASS && getblock(gx - 1, gy - 1, gz, blocks::ROCK, chunkindex) == blocks::GRASS) {
			tcx = Textures::getTexcoordX(blk[0], 1) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 1) + EPS;
		}
		else {
			tcx = Textures::getTexcoordX(blk[0], 2) + EPS;
			tcy = Textures::getTexcoordY(blk[0], 2) + EPS;
		}
		// Left Face
		if (!(BlockInfo(blk[4]).isOpaque() || (blk[4] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF) {

			colors = brt[4];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (colors + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
				color2 = (colors + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
				color3 = (colors + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy, gz + 1) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
				color4 = (colors + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy, gz - 1) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;
			if (blk[0] != blocks::GLOWSTONE){
				color1 *= 0.7;
				color2 *= 0.7;
				color3 *= 0.7;
				color4 *= 0.7;
			}

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*0.0); renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*0.0); renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*1.0); renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*1.0); renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);

		}

		tcx = Textures::getTexcoordX(blk[0], 1);
		tcy = Textures::getTexcoordY(blk[0], 1);

		// Top Face
		if (!(BlockInfo(blk[5]).isOpaque() || (blk[5] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF) {

			colors = brt[5];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (color1 + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy + 1, gz - 1)) / 4.0;
				color2 = (color2 + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx - 1, gy + 1, gz) + getbrightness(gx - 1, gy + 1, gz + 1)) / 4.0;
				color3 = (color3 + getbrightness(gx, gy + 1, gz + 1) + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy + 1, gz + 1)) / 4.0;
				color4 = (color4 + getbrightness(gx, gy + 1, gz - 1) + getbrightness(gx + 1, gy + 1, gz) + getbrightness(gx + 1, gy + 1, gz - 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*1.0); renderer::Vertex3d(-0.5 + x, 0.5 + y, -0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*0.0); renderer::Vertex3d(-0.5 + x, 0.5 + y, 0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*0.0); renderer::Vertex3d(0.5 + x, 0.5 + y, 0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*1.0); renderer::Vertex3d(0.5 + x, 0.5 + y, -0.5 + z);

		}

		tcx = Textures::getTexcoordX(blk[0], 3);
		tcy = Textures::getTexcoordY(blk[0], 3);

		// Bottom Face
		if (!(BlockInfo(blk[6]).isOpaque() || (blk[6] == blk[0] && BlockInfo(blk[0]).isOpaque() == false)) || blk[0] == blocks::LEAF) {

			colors = brt[6];
			color1 = colors; color2 = colors; color3 = colors; color4 = colors;

			if (blk[0] != blocks::GLOWSTONE && SmoothLighting) {
				color1 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy - 1, gz - 1)) / 4.0;
				color2 = (colors + getbrightness(gx, gy - 1, gz - 1) + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy - 1, gz - 1)) / 4.0;
				color3 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx + 1, gy - 1, gz) + getbrightness(gx + 1, gy - 1, gz + 1)) / 4.0;
				color4 = (colors + getbrightness(gx, gy - 1, gz + 1) + getbrightness(gx - 1, gy - 1, gz) + getbrightness(gx - 1, gy - 1, gz + 1)) / 4.0;
			}

			color1 /= BRIGHTNESSMAX;
			color2 /= BRIGHTNESSMAX;
			color3 /= BRIGHTNESSMAX;
			color4 /= BRIGHTNESSMAX;

			renderer::Color3d(color1, color1, color1);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*1.0); renderer::Vertex3d(-0.5 + x, -0.5 + y, -0.5 + z);
			renderer::Color3d(color2, color2, color2);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*1.0); renderer::Vertex3d(0.5 + x, -0.5 + y, -0.5 + z);
			renderer::Color3d(color3, color3, color3);
			renderer::TexCoord2d(tcx + size*0.0, tcy + size*0.0); renderer::Vertex3d(0.5 + x, -0.5 + y, 0.5 + z);
			renderer::Color3d(color4, color4, color4);
			renderer::TexCoord2d(tcx + size*1.0, tcy + size*0.0); renderer::Vertex3d(-0.5 + x, -0.5 + y, 0.5 + z);

		}
	}


	vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box){
		//返回与box相交的所有方块AABB

		Hitbox::AABB blockbox;
		vector<Hitbox::AABB> hitBoxes;

		for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++){
			for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++){
				for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++){ //? check
					if (BlockInfo(getblock(a, b, c)).isSolid()){
						blockbox.xmin = a - 0.5;
						blockbox.xmax = a + 0.5;
						blockbox.ymin = b - 0.5;
						blockbox.ymax = b + 0.5;
						blockbox.zmin = c - 0.5;
						blockbox.zmax = c + 0.5;
						if (Hitbox::Hit(box, blockbox)){
							hitBoxes.push_back(blockbox);
						}
					}
				}
			}
		}
		return hitBoxes;
	}

	bool inWater(Hitbox::AABB box){
		Hitbox::AABB blockbox;
		int a, b, c;
		for (a = int(box.xmin + 0.5) - 1; a != int(box.xmax + 0.5); a++){
			for (b = int(box.ymin + 0.5) - 1; b != int(box.ymax + 0.5); b++){
				for (c = int(box.zmin + 0.5) - 1; c != int(box.zmax + 0.5); c++){
					if (getblock(a, b, c) == blocks::WATER) {
						blockbox.xmin = a - 0.5;
						blockbox.xmax = a + 0.5;
						blockbox.ymin = b - 0.5;
						blockbox.ymax = b + 0.5;
						blockbox.zmin = c - 0.5;
						blockbox.zmax = c + 0.5;
						if (Hitbox::Hit(box, blockbox)) return true;
					}
				}
			}
		}
		return false;
	}

	void updateblock(int x, int y, int z, bool blockchanged){
		//Blockupdate

		int cx, cy, cz, bx, by, bz;
		bool updated = blockchanged;

		cx = getchunkpos(x);
		cy = getchunkpos(y);
		cz = getchunkpos(z);

		bx = getblockpos(x);
		by = getblockpos(y);
		bz = getblockpos(z);

		if (chunkOutOfBound(cx, cy, cz) == false){

			chunk* cptr = getChunkPtr(cx, cy, cz);
			if (cptr!=nullptr){
				brightness oldbrightness = cptr->getbrightness(bx, by, bz);
				bool skylighted = true;
				int yi, cyi;
				yi = y + 1; cyi = getchunkpos(yi);
				if (y < 0){
					skylighted = false;
				}
				else{
					while (chunkLoaded(cx, cyi + 1, cz) && skylighted){
						if (BlockInfo(getblock(x, yi, z)).isOpaque() || getblock(x, yi, z) == blocks::WATER){
							skylighted = false;
						}
						yi++; cyi = getchunkpos(yi);
					}
				}
				if (!BlockInfo(getblock(x, y, z)).isOpaque()){

					brightness br;
					int maxbrightness;
					block blks[7] = { 0,
						getblock(x, y, z + 1),    //Front face
						getblock(x, y, z - 1),    //Back face
						getblock(x + 1, y, z),    //Right face
						getblock(x - 1, y, z),    //Left face
						getblock(x, y + 1, z),    //Top face
						getblock(x, y - 1, z) };     //Bottom face
					brightness brts[7] = { 0,
						getbrightness(x, y, z + 1),    //Front face
						getbrightness(x, y, z - 1),    //Back face
						getbrightness(x + 1, y, z),    //Right face
						getbrightness(x - 1, y, z),    //Left face
						getbrightness(x, y + 1, z),    //Top face
						getbrightness(x, y - 1, z) };     //Bottom face
					maxbrightness = 1;
					for (int i = 2; i != 6; i++){
						if (brts[maxbrightness] < brts[i]) maxbrightness = i;
					}
					br = brts[maxbrightness];
					if (blks[maxbrightness] == blocks::WATER){
						if (br - 2 < BRIGHTNESSMIN) br = BRIGHTNESSMIN; else br -= 2;
					}
					else{
						if (br - 1 < BRIGHTNESSMIN) br = BRIGHTNESSMIN; else br--;
					}

					if (skylighted){
						if (br < skylight) br = skylight;
					}
					if (br < BRIGHTNESSMIN) br = BRIGHTNESSMIN;
					//Set brightness
					cptr->setbrightness(bx, by, bz, br);

				}
				else{

					//Opaque block
					getChunkPtr(cx, cy, cz)->setbrightness(bx, by, bz, 0);
					if (getblock(x, y, z) == blocks::GLOWSTONE || getblock(x, y, z) == blocks::LAVA){
						cptr->setbrightness(bx, by, bz, BRIGHTNESSMAX);
					}

				}


				if (oldbrightness != cptr->getbrightness(bx, by, bz)) updated = true;

				if (updated){
					updateblock(x, y + 1, z, false);
					updateblock(x, y - 1, z, false);
					updateblock(x + 1, y, z, false);
					updateblock(x - 1, y, z, false);
					updateblock(x, y, z + 1, false);
					updateblock(x, y, z - 1, false);
				}

				setChunkUpdated(cx, cy, cz, true);
				if (bx == 15 && cx<worldsize - 1) setChunkUpdated(cx + 1, cy, cz, true);
				if (bx == 0 && cx>-worldsize) setChunkUpdated(cx - 1, cy, cz, true);
				if (by == 15 && cy<worldheight - 1) setChunkUpdated(cx, cy + 1, cz, true);
				if (by == 0 && cy>-worldheight) setChunkUpdated(cx, cy - 1, cz, true);
				if (bz == 15 && cz<worldsize - 1) setChunkUpdated(cx, cy, cz + 1, true);
				if (bz == 0 && cz>-worldsize) setChunkUpdated(cx, cy, cz - 1, true);

			}
		}
	}

	block getblock(int x, int y, int z, block mask, int cindex){
		//获取XYZ的方块
		int cx, cy, cz;
		cx = getchunkpos(x);cy = getchunkpos(y);cz = getchunkpos(z);
		if (chunkOutOfBound(cx, cy, cz))return blocks::AIR;
		int bx, by, bz;
		bx = getblockpos(x);by = getblockpos(y);bz = getblockpos(z);
		if (cindex != -1 && cx == chunks[cindex].cx && cy == chunks[cindex].cy && cz == chunks[cindex].cz){
			return chunks[cindex].getblock(bx, by, bz);
		}
		int ci = getChunkIndex(cx, cy, cz);
		if (ci != -1)return chunks[ci].getblock(bx, by, bz);
		return mask;
	}

	brightness getbrightness(int x, int y, int z, int cindex){

		//获取XYZ的亮度
		int cx, cy, cz;
		cx = getchunkpos(x);cy = getchunkpos(y);cz = getchunkpos(z);
		if (chunkOutOfBound(cx, cy, cz))return skylight;
		int bx, by, bz;
		bx = getblockpos(x);by = getblockpos(y);bz = getblockpos(z);
		if (cindex != -1 && cx == chunks[cindex].cx && cy == chunks[cindex].cy && cz == chunks[cindex].cz){
			return chunks[cindex].getbrightness(bx, by, bz);
		}
		int ci = getChunkIndex(cx, cy, cz);
		if (ci != -1)return chunks[ci].getbrightness(bx, by, bz);
		return skylight;

	}

	void setblock(int x, int y, int z, block Blockname){

		//设置方块
		int cx, cy, cz, bx, by, bz;

		cx = getchunkpos(x);
		cy = getchunkpos(y);
		cz = getchunkpos(z);

		bx = getblockpos(x);
		by = getblockpos(y);
		bz = getblockpos(z);

		if (!chunkOutOfBound(cx, cy, cz)){
			int i = getChunkIndex(cx, cy, cz);
			if (i != -1){
				chunks[i].setblock(bx, by, bz, Blockname);
				updateblock(x, y, z, true);
			}
		}

	}

	void setbrightness(int x, int y, int z, brightness Brightness){

		//设置XYZ的亮度
		int cx, cy, cz, bx, by, bz;

		cx = getchunkpos(x);
		cy = getchunkpos(y);
		cz = getchunkpos(z);

		bx = getblockpos(x);
		by = getblockpos(y);
		bz = getblockpos(z);

		if (!chunkOutOfBound(cx, cy, cz)){
			int i = getChunkIndex(cx, cy, cz);
			if (i != -1){
				chunks[i].setbrightness(bx, by, bz, Brightness);
			}
		}

	}

	void putblock(int x, int y, int z, block Blockname){

		//这个void和上面那个是一样的，只是本人的完美主义（说白了就是强迫症）驱使我再写一遍= =
		//是不是感觉这句话有些眼熟。。。
		setblock(x, y, z, Blockname);

	}

	void pickblock(int x, int y, int z){

		//这个void根本没有存在的必要，其功能等于setblock(x,y,z,0)或putblock(x,y,z,0),但是本人的完...
		//是不是感觉这句话还是有些眼熟。。。
		setblock(x, y, z, blocks::AIR);

	}

	//为什么之前那些话都有些眼熟呢？？？
	//原来是因为这里的set/put/pickblock三个Sub是世界范围的，而之前的是区块范围的。。。

	bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist){

		//检测给出的chunk坐标是否在渲染范围内
		if (x<px - dist || x>px + dist - 1 || y<py - dist || y>py + dist - 1 || z<pz - dist || z>pz + dist - 1){
			return false;
		}
		return true;

	}

	void sortChunkBuildRenderList(int xpos, int ypos, int zpos) {
		int cxp, cyp, czp, cx, cy, cz, p = 0;
		int xd, yd, zd, distsqr;

		cxp = getchunkpos(xpos);
		cyp = getchunkpos(ypos);
		czp = getchunkpos(zpos);

		for (int ci = 0; ci < loadedChunks; ci++) {
			if (chunks[ci].updated) {
				cx = chunks[ci].cx;
				cy = chunks[ci].cy;
				cz = chunks[ci].cz;
				if (!chunkInRange(cx, cy, cz, cxp, cyp, czp, viewdistance)) continue;
				xd = cx * 16 + 7 - xpos;
				yd = cy * 16 + 7 - ypos;
				zd = cz * 16 + 7 - zpos;
				distsqr = xd*xd + yd*yd + zd*zd;
				for (int i = 0; i < 4; i++) {
					if (distsqr < chunkBuildRenderList[i][0] || p <= i) {
						for (int j = 3; j >= i + 1; j--) {
							chunkBuildRenderList[j][0] = chunkBuildRenderList[j - 1][0];
							chunkBuildRenderList[j][1] = chunkBuildRenderList[j - 1][1];
						}
						chunkBuildRenderList[i][0] = distsqr;
						chunkBuildRenderList[i][1] = ci;
						break;
					}
				}
				if (p < 4) p++;
			}
		}
		chunkBuildRenders = p;
	}

	void sortChunkLoadUnloadList(int xpos, int ypos, int zpos) {

		int cxp, cyp, czp, cx, cy, cz, pl = 0, pu = 0, i, cxt, cyt, czt;
		int xd, yd, zd, distsqr, first, middle, last;
		int lcasize = (viewdistance + 1) * 2;
		int lcadelta = viewdistance + 1;
		if (loadedChunkArray == nullptr){
			loadedChunkArray = new bool[lcasize*lcasize*lcasize];
		}
		memset(loadedChunkArray, false, lcasize*lcasize*lcasize*sizeof(bool));

		cxp = getchunkpos(xpos);
		cyp = getchunkpos(ypos);
		czp = getchunkpos(zpos);

		for (int ci = 0; ci < loadedChunks; ci++) {
			cx = chunks[ci].cx;
			cy = chunks[ci].cy;
			cz = chunks[ci].cz;
			if (!chunkInRange(cx, cy, cz, cxp, cyp, czp, viewdistance + 1)) {
				xd = cx * 16 + 7 - xpos;
				yd = cy * 16 + 7 - ypos;
				zd = cz * 16 + 7 - zpos;
				distsqr = xd*xd + yd*yd + zd*zd;

				first = 0; last = pl - 1;
				while (first <= last) {
					middle = (first + last) / 2;
					if (distsqr > chunkUnloadList[middle][0])last = middle - 1;
					else first = middle + 1;
				}
				if (first > pl || first >= 4) continue;
				i = first;

				for (int j = 3; j > i; j--) {
					chunkUnloadList[j][0] = chunkUnloadList[j - 1][0];
					chunkUnloadList[j][1] = chunkUnloadList[j - 1][1];
					chunkUnloadList[j][2] = chunkUnloadList[j - 1][2];
					chunkUnloadList[j][3] = chunkUnloadList[j - 1][3];
				}
				chunkUnloadList[i][0] = distsqr;
				chunkUnloadList[i][1] = cx;
				chunkUnloadList[i][2] = cy;
				chunkUnloadList[i][3] = cz;

				if (pl < 4) pl++;
			}
			else {
				cxt = cx - cxp + lcadelta;
				cyt = cy - cyp + lcadelta;
				czt = cz - czp + lcadelta;
				loadedChunkArray[cxt*lcasize*lcasize + cyt*lcasize + czt] = true;
			}
		}
		chunkUnloads = pl;

		for (cx = cxp - viewdistance - 1; cx <= cxp + viewdistance; cx++) {
			for (cy = cyp - viewdistance - 1; cy <= cyp + viewdistance; cy++) {
				for (cz = czp - viewdistance - 1; cz <= czp + viewdistance; cz++) {
					cxt = cx - cxp + lcadelta;
					cyt = cy - cyp + lcadelta;
					czt = cz - czp + lcadelta;
					if (!chunkOutOfBound(cx, cy, cz)) {
						if (!loadedChunkArray[cxt*lcasize*lcasize + cyt*lcasize + czt]) {
							xd = cx * 16 + 7 - xpos;
							yd = cy * 16 + 7 - ypos;
							zd = cz * 16 + 7 - zpos;
							distsqr = xd *xd + yd *yd + zd *zd;

							first = 0; last = pu - 1;
							while (first <= last) {
								middle = (first + last) / 2;
								if (distsqr < chunkLoadList[middle][0])last = middle - 1;
								else first = middle + 1;
							}
							if (first > pu || first >= 4)  continue;
							i = first;

							for (int j = 3; j > i; j--) {
								chunkLoadList[j][0] = chunkLoadList[j - 1][0];
								chunkLoadList[j][1] = chunkLoadList[j - 1][1];
								chunkLoadList[j][2] = chunkLoadList[j - 1][2];
								chunkLoadList[j][3] = chunkLoadList[j - 1][3];
							}
							chunkLoadList[i][0] = distsqr;
							chunkLoadList[i][1] = cx;
							chunkLoadList[i][2] = cy;
							chunkLoadList[i][3] = cz;
							if (pu < 4) pu++;
						}
					}
				}
			}
		}
		chunkLoads = pu;
	}
	
	void saveAllChunks(){
#ifndef NEWORLD_DEBUG_NO_FILEIO
		int i;
		for (i = 0; i != loadedChunks ; i++){
			chunks[i].SaveToFile();
		}
#endif
	}

	void destroyAllChunks(){

		for (int i = 0; i != loadedChunks ; i++){
			if (!chunks[i].Empty){
				chunks[i].destroyRender();
				chunks[i].destroy();
			}
		}
		free(chunks);
		chunks = nullptr;
		loadedChunks = 0;
		chunkArraySize = 0;
		if(ciArrayAval)ciArray.destroy();
		HMap.destroy();

		rebuiltChunks = 0; rebuiltChunksCount = 0;
		updatedChunks = 0; updatedChunksCount = 0;
		unloadedChunks = 0; unloadedChunksCount = 0;
		memset(chunkBuildRenderList, 0, 256 * 2 * sizeof(int));
		memset(chunkLoadList, 0, 256 * 4 * sizeof(int));
		memset(chunkUnloadList, 0, 256 * 4 * sizeof(int));
		chunkBuildRenders = 0; chunkLoads = 0; chunkUnloads = 0;
		
		delete[] loadedChunkArray;
		loadedChunkArray = nullptr;
	}

	void buildtree(int x, int y, int z){

		block trblock = getblock(x, y, z), tublock = getblock(x, y - 1, z);
		ubyte xt, yt, zt;
		ubyte th = ubyte(rnd() * 3) + 4;
		if (trblock != blocks::AIR || tublock != blocks::GRASS || tublock == blocks::AIR) return;

		for (yt = 0; yt != th; yt++){
			setblock(x, y + yt, z, blocks::WOOD);
		}

		setblock(x, y - 1, z, blocks::DIRT);

		for (xt = 0; xt != 4; xt++){
			for (zt = 0; zt != 4; zt++){
				for (yt = 0; yt != 1; yt++){
					if (getblock(x + xt - 2, y + th - 2 - yt, z + zt - 2) == blocks::AIR) setblock(x + xt - 2, y + th - 2 - yt, z + zt - 2, blocks::LEAF);
				}
			}
		}

		for (xt = 0; xt != 2; xt++){
			for (zt = 0; zt != 2; zt++){
				for (yt = 0; yt != 1; yt++){
					if (getblock(x + xt - 1, y + th - 1 + yt, z + zt - 1) == blocks::AIR && abs(xt - 1) != abs(zt - 1)) setblock(x + xt - 1, y + th - 1 + yt, z + zt - 1, blocks::LEAF);
				}
			}
		}

		setblock(x, y + th, z, blocks::LEAF);

	}

}
