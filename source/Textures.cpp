#include "Textures.h"
#include "Items.h"
#include "Blocks.h"
#include <fstream>

namespace Textures {

    const TextureIndex Indexes[Blocks::BLOCKS_COUNT][3] = {
        { WHITE,WHITE,WHITE },
        { ROCK,ROCK,ROCK },
        { GRASS_TOP,GRASS_SIDE,DIRT },
        { DIRT,DIRT,DIRT },
        { STONE,STONE,STONE },
        { PLANK,PLANK,PLANK },
        { WOOD_TOP,WOOD_SIDE,WOOD_TOP },
        { BEDROCK,BEDROCK,BEDROCK },
        { LEAF,LEAF,LEAF },
        { GLASS,GLASS,GLASS },
        { WATER,WATER,WATER },
        { LAVA,LAVA,LAVA },
        { GLOWSTONE,GLOWSTONE,GLOWSTONE },
        { SAND,SAND,SAND },
        { CEMENT,CEMENT,CEMENT },
        { ICE,ICE,ICE },
        { COAL,COAL,COAL },
        { IRON,IRON,IRON },
        { TNT,TNT,TNT },
        { NULLBLOCK, NULLBLOCK, NULLBLOCK },
    };

    TextureIndex getTextureIndex(BlockID blockname, uint8_t side) {
        side--;
        if (blockname >= Blocks::BLOCKS_COUNT || side >= 3) return NULLBLOCK;
        return Indexes[blockname][side];
    }

    void LoadRGBImage(ImageRGB& tex, string Filename) {
        unsigned int ind = 0;
        ImageRGB& bitmap = tex; // 返回位图
        bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
        std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in); // 位图文件（二进制）
        if (!bmpfile.is_open()) {
            printf("[console][Warning] Cannot load %s\n", Filename.c_str());
            return;
        }
        BitmapFileHeader bfh; // 各种关于文件的参数
        BitmapInfoHeader bih; // 各种关于位图的参数
        // 开始读取
        bmpfile.read((char*)&bfh, sizeof(BitmapFileHeader));
        bmpfile.read((char*)&bih, sizeof(BitmapInfoHeader));
        bitmap.sizeX = bih.biWidth;
        bitmap.sizeY = bih.biHeight;
        bitmap.buffer = std::unique_ptr<uint8_t[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 3]);
        bmpfile.read((char*)bitmap.buffer.get(), bitmap.sizeX*bitmap.sizeY * 3);
        bmpfile.close();
        for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
            unsigned char t = bitmap.buffer[ind];
            bitmap.buffer[ind] = bitmap.buffer[ind + 2];
            bitmap.buffer[ind + 2] = t;
            ind += 3;
        }
    }

    void LoadRGBAImage(ImageRGBA& tex, string Filename, string MkFilename) {
        unsigned char *rgb = nullptr, *a = nullptr;
        unsigned int ind = 0;
        bool noMaskFile = (MkFilename == "");
        ImageRGBA& bitmap = tex;
        bitmap.buffer = nullptr; bitmap.sizeX = bitmap.sizeY = 0;
        std::ifstream bmpfile(Filename, std::ios::binary | std::ios::in);
        std::ifstream maskfile;
        if (!noMaskFile) maskfile.open(MkFilename, std::ios::binary | std::ios::in);
        if (!bmpfile.is_open()) {
            std::stringstream ss; ss << "Cannot load bitmap " << Filename;
            DebugWarning(ss.str()); return;
        }
        if (!noMaskFile && !maskfile.is_open()) {
            std::stringstream ss; ss << "Cannot load bitmap " << MkFilename;
            DebugWarning(ss.str()); return;
        }
        BitmapFileHeader bfh;
        BitmapInfoHeader bih;
        if (!noMaskFile) {
            maskfile.read((char*)&bfh, sizeof(BitmapFileHeader));
            maskfile.read((char*)&bih, sizeof(BitmapInfoHeader));
        }
        bmpfile.read((char*)&bfh, sizeof(BitmapFileHeader));
        bmpfile.read((char*)&bih, sizeof(BitmapInfoHeader));
        bitmap.sizeX = bih.biWidth;
        bitmap.sizeY = bih.biHeight;
        bitmap.buffer = std::unique_ptr<uint8_t[]>(new unsigned char[bitmap.sizeX * bitmap.sizeY * 4]);
        rgb = new unsigned char[bitmap.sizeX * bitmap.sizeY * 3];
        bmpfile.read((char*)rgb, bitmap.sizeX*bitmap.sizeY * 3);
        bmpfile.close();
        if (!noMaskFile) {
            a = new unsigned char[bitmap.sizeX*bitmap.sizeY * 3];
            maskfile.read((char*)a, bitmap.sizeX*bitmap.sizeY * 3);
            maskfile.close();
        }
        for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
            bitmap.buffer[ind] = rgb[i * 3 + 2];
            bitmap.buffer[ind + 1] = rgb[i * 3 + 1];
            bitmap.buffer[ind + 2] = rgb[i * 3];
            if (noMaskFile) bitmap.buffer[ind + 3] = 255;
            else bitmap.buffer[ind + 3] = 255 - a[i * 3];
            ind += 4;
        }
    }

    TextureID LoadRGBTexture(string Filename, bool bilinear) {
        ImageRGB image;
        TextureID ret;
        LoadRGBImage(image, Filename);
        glGenTextures(1, &ret);
        glBindTexture(GL_TEXTURE_2D, ret);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.sizeX, image.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image.buffer.get());
        glGenerateMipmap(GL_TEXTURE_2D);
        return ret;
    }

    TextureID LoadRGBATexture(string Filename, string MkFilename, bool bilinear) {
        TextureID ret;
        ImageRGBA image;
        LoadRGBAImage(image, Filename, MkFilename);
        glGenTextures(1, &ret);
        glBindTexture(GL_TEXTURE_2D, ret);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.sizeX, image.sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.buffer.get());
        glGenerateMipmap(GL_TEXTURE_2D);
        return ret;
    }

    TextureID LoadFontTexture(string Filename) {
        ImageRGBA Texture;
        ImageRGB image;
        uint8_t *ip, *tp;
        TextureID ret;
        LoadRGBImage(image, Filename);
        Texture.sizeX = image.sizeX;
        Texture.sizeY = image.sizeY;
        Texture.buffer = std::unique_ptr<uint8_t[]>(new unsigned char[image.sizeX * image.sizeY * 4]);
        if (Texture.buffer == nullptr) {
            printf("[console][Warning] Cannot alloc memory when loading %s\n", Filename.c_str());
            return 0;
        }
        ip = image.buffer.get();
        tp = Texture.buffer.get();
        for (unsigned int i = 0; i != image.sizeX*image.sizeY; i++) {
            *tp = 255; tp++;
            *tp = 255; tp++;
            *tp = 255; tp++;
            *tp = 255 - *ip; tp++; ip += 3;
        }
        glGenTextures(1, &ret);
        glBindTexture(GL_TEXTURE_2D, ret);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Texture.sizeX, Texture.sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, Texture.buffer.get());
        glGenerateMipmap(GL_TEXTURE_2D);
        return ret;
    }

    TextureID LoadBlockTextureArray(string Filename, string MkFilename) {
        TextureID ret;
        ImageRGBA image;
        LoadRGBAImage(image, Filename, MkFilename);
        int size = image.sizeX;
        int count = image.sizeY / size;
        glGenTextures(1, &ret);
        glBindTexture(GL_TEXTURE_2D_ARRAY, ret);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
        // glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, mipmapLevel);
        // glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_LOD, 0);
        // glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LOD, mipmapLevel);
        // glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, 0.0);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, size, size, count, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.buffer.get());
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        return ret;
    }

    void SaveRGBImage(string filename, ImageRGB& image) {
        BitmapFileHeader bitmapfileheader;
        BitmapInfoHeader bitmapinfoheader;
        bitmapfileheader.bfSize = image.sizeX*image.sizeY * 3 + 54;
        bitmapinfoheader.biWidth = image.sizeX;
        bitmapinfoheader.biHeight = image.sizeY;
        bitmapinfoheader.biSizeImage = image.sizeX*image.sizeY * 3;
        for (unsigned int i = 0; i != image.sizeX*image.sizeY * 3; i += 3) {
            uint8_t t = image.buffer.get()[i];
            image.buffer.get()[i] = image.buffer.get()[i + 2];
            image.buffer.get()[i + 2] = t;
        }
        std::ofstream ofs(filename, std::ios::out | std::ios::binary);
        ofs.write((char*)&bitmapfileheader, sizeof(bitmapfileheader));
        ofs.write((char*)&bitmapinfoheader, sizeof(bitmapinfoheader));
        ofs.write((char*)image.buffer.get(), sizeof(uint8_t)*image.sizeX*image.sizeY * 3);
        ofs.close();
    }
}
