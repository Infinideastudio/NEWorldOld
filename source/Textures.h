#pragma once
#include "Definitions.h"

const short BITMAP_ID = 0x4D42;

namespace Textures {
	struct ImageRGB {
		unsigned int sizeX = 0;
		unsigned int sizeY = 0;
		std::unique_ptr<uint8_t[]> buffer;
	};

	struct ImageRGBA {
		unsigned int sizeX = 0;
		unsigned int sizeY = 0;
		std::unique_ptr<uint8_t[]> buffer;
	};

#pragma pack(push)
#pragma pack(1)
	struct BitmapFileHeader {
		short bfType = BITMAP_ID;
		int bfSize;
		short bfReserved1 = 0, bfReserved2 = 0;
		int bfOffBits = 54;
	};

	struct BitmapInfoHeader {
		int biSize = 40, biWidth, biHeight;
		short biPlanes = 1, biBitCount = 24;
		int biCompression = 0, biSizeImage, biXPelsPerMeter = 0, biYPelsPerMeter = 0, biClrUsed = 0, biClrImportant = 0;
	};
#pragma pack(pop)

	enum TextureIndices: TextureIndex {
		WHITE, BREAKING_0, BREAKING_1, BREAKING_2, BREAKING_3, BREAKING_4, BREAKING_5, BREAKING_6, BREAKING_7, NULLBLOCK,
		ROCK, GRASS_TOP, GRASS_SIDE, DIRT, STONE, PLANK, WOOD_TOP, WOOD_SIDE, BEDROCK, LEAF,
		GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON, TNT,
		TEXTURE_INDICES_COUNT
	};

	TextureIndex getTextureIndex(BlockID blockname, size_t face);
	void LoadRGBImage(ImageRGB& tex, string Filename);
	void LoadRGBAImage(ImageRGBA& tex, string Filename, string MkFilename);

	TextureID LoadRGBTexture(string Filename, bool bilinear = false);
	TextureID LoadRGBATexture(string Filename, string MkFilename, bool bilinear = false);
	TextureID LoadBlockTextureArray(string Filename, string MkFilename);
	void SaveRGBImage(string filename, ImageRGB const& image);
}
