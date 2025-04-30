#include "Textures.h"
#include <fstream>
#include "Blocks.h"

namespace Textures {

const TextureIndex Indexes[Blocks::BLOCKS_COUNT][3] = {
    {    WHITE,      WHITE,     WHITE},
    {     ROCK,       ROCK,      ROCK},
    {GRASS_TOP, GRASS_SIDE,      DIRT},
    {     DIRT,       DIRT,      DIRT},
    {    STONE,      STONE,     STONE},
    {    PLANK,      PLANK,     PLANK},
    { WOOD_TOP,  WOOD_SIDE,  WOOD_TOP},
    {  BEDROCK,    BEDROCK,   BEDROCK},
    {     LEAF,       LEAF,      LEAF},
    {    GLASS,      GLASS,     GLASS},
    {    WATER,      WATER,     WATER},
    {     LAVA,       LAVA,      LAVA},
    {GLOWSTONE,  GLOWSTONE, GLOWSTONE},
    {     SAND,       SAND,      SAND},
    {   CEMENT,     CEMENT,    CEMENT},
    {      ICE,        ICE,       ICE},
    {     COAL,       COAL,      COAL},
    {     IRON,       IRON,      IRON},
    {      TNT,        TNT,       TNT},
    {NULLBLOCK,  NULLBLOCK, NULLBLOCK},
};

TextureIndex getTextureIndex(BlockID blockname, size_t face) {
    if (blockname >= Blocks::BLOCKS_COUNT || face >= 3)
        return NULLBLOCK;
    return Indexes[blockname][face];
}

void LoadRGBImage(ImageRGB& tex, std::filesystem::path const& path) {
    unsigned int ind = 0;
    ImageRGB& bitmap = tex; // 返回位图
    bitmap.buffer = nullptr;
    bitmap.sizeX = bitmap.sizeY = 0;
    std::ifstream bmpfile(path, std::ios::binary | std::ios::in); // 位图文件（二进制）
    if (!bmpfile.is_open()) {
        std::stringstream ss;
        ss << "Cannot load bitmap " << path;
        DebugWarning(ss.str());
        return;
    }
    BitmapFileHeader bfh; // 各种关于文件的参数
    BitmapInfoHeader bih; // 各种关于位图的参数
    // 开始读取
    bmpfile.read((char*) &bfh, sizeof(BitmapFileHeader));
    bmpfile.read((char*) &bih, sizeof(BitmapInfoHeader));
    bitmap.sizeX = bih.biWidth;
    bitmap.sizeY = bih.biHeight;
    bitmap.buffer = std::make_unique<uint8_t[]>(bitmap.sizeX * bitmap.sizeY * 3);
    bmpfile.read((char*) bitmap.buffer.get(), bitmap.sizeX * bitmap.sizeY * 3);
    bmpfile.close();
    for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
        unsigned char t = bitmap.buffer[ind];
        bitmap.buffer[ind] = bitmap.buffer[ind + 2];
        bitmap.buffer[ind + 2] = t;
        ind += 3;
    }
}

void LoadRGBAImage(ImageRGBA& tex, std::filesystem::path const& path, std::filesystem::path const& maskPath) {
    unsigned int ind = 0;
    bool noMaskFile = (maskPath == "");
    ImageRGBA& bitmap = tex;
    bitmap.buffer = nullptr;
    bitmap.sizeX = bitmap.sizeY = 0;
    std::ifstream bmpfile(path, std::ios::binary | std::ios::in);
    std::ifstream maskfile;
    if (!noMaskFile)
        maskfile.open(maskPath, std::ios::binary | std::ios::in);
    if (!bmpfile.is_open()) {
        std::stringstream ss;
        ss << "Cannot load bitmap " << path;
        DebugWarning(ss.str());
        return;
    }
    if (!noMaskFile && !maskfile.is_open()) {
        std::stringstream ss;
        ss << "Cannot load bitmap " << maskPath;
        DebugWarning(ss.str());
        return;
    }
    BitmapFileHeader bfh;
    BitmapInfoHeader bih;
    if (!noMaskFile) {
        maskfile.read((char*) &bfh, sizeof(BitmapFileHeader));
        maskfile.read((char*) &bih, sizeof(BitmapInfoHeader));
    }
    bmpfile.read((char*) &bfh, sizeof(BitmapFileHeader));
    bmpfile.read((char*) &bih, sizeof(BitmapInfoHeader));

    bitmap.sizeX = bih.biWidth;
    bitmap.sizeY = bih.biHeight;
    bitmap.buffer = std::make_unique<uint8_t[]>(bitmap.sizeX * bitmap.sizeY * 4);

    auto rgb = std::make_unique<uint8_t[]>(bitmap.sizeX * bitmap.sizeY * 3);
    auto alpha = std::make_unique<uint8_t[]>(bitmap.sizeX * bitmap.sizeY * 3);

    bmpfile.read((char*) rgb.get(), bitmap.sizeX * bitmap.sizeY * 3);
    bmpfile.close();
    if (!noMaskFile) {
        maskfile.read((char*) alpha.get(), bitmap.sizeX * bitmap.sizeY * 3);
        maskfile.close();
    }
    for (unsigned int i = 0; i < bitmap.sizeX * bitmap.sizeY; i++) {
        bitmap.buffer[ind] = rgb[i * 3 + 2];
        bitmap.buffer[ind + 1] = rgb[i * 3 + 1];
        bitmap.buffer[ind + 2] = rgb[i * 3];
        if (noMaskFile)
            bitmap.buffer[ind + 3] = 255;
        else
            bitmap.buffer[ind + 3] = 255 - alpha[i * 3];
        ind += 4;
    }
}

TextureID LoadRGBTexture(std::filesystem::path const& path, bool bilinear) {
    ImageRGB image;
    TextureID ret;
    LoadRGBImage(image, path);
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.sizeX, image.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, image.buffer.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    return ret;
}

TextureID LoadRGBATexture(std::filesystem::path const& path, std::filesystem::path const& maskPath, bool bilinear) {
    TextureID ret;
    ImageRGBA image;
    LoadRGBAImage(image, path, maskPath);
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_2D, ret);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.sizeX, image.sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.buffer.get());
    glGenerateMipmap(GL_TEXTURE_2D);
    return ret;
}

TextureID LoadBlockTextureArray(std::filesystem::path const& path, std::filesystem::path const& maskPath) {
    TextureID ret;
    ImageRGBA image;
    LoadRGBAImage(image, path, maskPath);
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

void SaveRGBImage(std::filesystem::path const& path, ImageRGB const& image) {
    BitmapFileHeader bitmapfileheader;
    BitmapInfoHeader bitmapinfoheader;
    bitmapfileheader.bfSize = image.sizeX * image.sizeY * 3 + 54;
    bitmapinfoheader.biWidth = image.sizeX;
    bitmapinfoheader.biHeight = image.sizeY;
    bitmapinfoheader.biSizeImage = image.sizeX * image.sizeY * 3;
    for (unsigned int i = 0; i != image.sizeX * image.sizeY * 3; i += 3) {
        uint8_t t = image.buffer.get()[i];
        image.buffer.get()[i] = image.buffer.get()[i + 2];
        image.buffer.get()[i + 2] = t;
    }
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    ofs.write((char*) &bitmapfileheader, sizeof(bitmapfileheader));
    ofs.write((char*) &bitmapinfoheader, sizeof(bitmapinfoheader));
    ofs.write((char*) image.buffer.get(), sizeof(uint8_t) * image.sizeX * image.sizeY * 3);
    ofs.close();
}
}
