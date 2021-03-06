#pragma once
#include "Definitions.h"
#include "Blocks.h"

extern int BLOCKTEXTURE_SIZE, BLOCKTEXTURE_UNITSIZE, filter;
const short BITMAP_ID = 0x4D42;

namespace Textures
{

#pragma pack(push)
#pragma pack(1)
    struct TEXTURE_RGB
    {
        unsigned int sizeX;
        unsigned int sizeY;
        unique_ptr<ubyte[]> buffer;
    };

    struct TEXTURE_RGBA
    {
        unsigned int sizeX;
        unsigned int sizeY;
        unique_ptr<ubyte[]> buffer;
    };

    struct BITMAPINFOHEADER
    {
        int biSize = 40, biWidth, biHeight;
        short biPlanes = 1, biBitCount = 24;
        int biCompression = 0, biSizeImage, biXPelsPerMeter = 0, biYPelsPerMeter = 0, biClrUsed = 0, biClrImportant = 0;
    };

    struct BITMAPFILEHEADER
    {
        short bfType = BITMAP_ID;
        int bfSize;
        short bfReserved1 = 0, bfReserved2 = 0;
        int bfOffBits = 54;
    };
#pragma pack(pop)

    enum
    {
        AIR, ROCK, GRASS_TOP, GRASS_SIDE, DIRT, STONE, PLANK, WOOD_TOP, WOOD_SIDE, BEDROCK, LEAF,
        GLASS, WATER, LAVA, GLOWSTONE, SAND, CEMENT, ICE, COAL, IRON, UNKNOWN
    };

    void Init();
    ubyte getTextureIndex(block blockname, ubyte side);
    double getTexcoordX(block iblock, ubyte side);
    double getTexcoordY(block iblock, ubyte side);
    void LoadRGBImage(TEXTURE_RGB &tex,const string& Filename);
    void LoadRGBAImage(TEXTURE_RGBA &tex,const string& Filename, const string& MkFilename);
    TextureID LoadRGBTexture(const string& Filename);
    TextureID LoadRGBATexture(const string& Filename, const string& MkFilename);
    TextureID LoadFontTexture(const string& filename);
    TextureID LoadFontTexture(int id);
    TextureID LoadBlock3DTexture(const string& Filename, const string& MkFilename);
    void SaveRGBImage(const string& filename, TEXTURE_RGB &image);

}
