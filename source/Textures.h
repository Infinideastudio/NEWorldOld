#pragma once
#include "Definitions.h"
#include "Blocks.h"

extern int BLOCKTEXTURE_SIZE, BLOCKTEXTURE_UNITSIZE, filter;
const short BITMAP_ID = 0x4D42;

namespace Textures{

#pragma pack(push)
#pragma pack(1)
	struct TEXTURE_RGB {
		unsigned int sizeX;
		unsigned int sizeY;
		unique_ptr<ubyte[]> buffer;
	};

	struct TEXTURE_RGBA {
		unsigned int sizeX;
		unsigned int sizeY;
		unique_ptr<ubyte[]> buffer;
	};

	struct BITMAPINFOHEADER {
		int biSize, biWidth, biHeight;
		short biPlanes, biBitCount;
		int biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter, biClrUsed, biClrImportant;
	};

	struct BITMAPFILEHEADER {
		short bfType;
		int bfSize;
		short bfReserved1, bfReserved2;
		int bfOffBits;
	};
#pragma pack(pop)

	enum {
		AIR, ROCK, GRASS_TOP, GRASS_SIDE, DIRT, STONE, PLANK, WOOD_TOP, WOOD_SIDE, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON, UNKNOWN
	};

	void Init();

	ubyte getTextureIndex(block blockname, ubyte side);

	double getTexcoordX(block iblock, ubyte side);

	double getTexcoordY(block iblock, ubyte side);

	TEXTURE_RGB LoadRGBImage(string Filename);

	TEXTURE_RGBA LoadRGBAImage(string Filename, string MkFilename);

	TextureID LoadRGBTexture(string Filename);

	TextureID LoadFontTexture(string Filename);

	TextureID LoadRGBATexture(string Filename, string MkFilename);

	void SaveRGBImage(string filename, TEXTURE_RGB& image);

}
