module;

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <glad/gl.h>

export module textures;
import types;
import blocks;
import globals;

export using TextureID = GLuint;

export TextureID SplashTexture;
export TextureID TitleTexture;
export TextureID UIBackgroundTextures[6];
export TextureID SelectedTexture;
export TextureID UnselectedTexture;
export TextureID BlockTextureArray;

export enum class TextureIndex: uint16_t {
    WHITE,
    BREAKING_0,
    BREAKING_1,
    BREAKING_2,
    BREAKING_3,
    BREAKING_4,
    BREAKING_5,
    BREAKING_6,
    BREAKING_7,
    NULLBLOCK,
    ROCK,
    GRASS_TOP,
    GRASS_SIDE,
    DIRT,
    STONE,
    PLANK,
    WOOD_TOP,
    WOOD_SIDE,
    BEDROCK,
    LEAF,
    GLASS,
    WATER,
    LAVA,
    GLOWSTONE,
    SAND,
    CEMENT,
    ICE,
    COAL,
    IRON,
    TNT,
    TEXTURE_INDICES_COUNT
};

namespace Textures {

export struct ImageRGB {
    uint32_t sizeX = 0;
    uint32_t sizeY = 0;
    std::unique_ptr<uint8_t[]> buffer;
};

export struct ImageRGBA {
    uint32_t sizeX = 0;
    uint32_t sizeY = 0;
    std::unique_ptr<uint8_t[]> buffer;
};

#pragma pack(push)
#pragma pack(1)
constexpr uint16_t BITMAP_ID = 0x4D42;

struct BitmapInfoHeader {
    uint32_t biSize = sizeof(BitmapInfoHeader), biWidth = 0, biHeight = 0;
    uint16_t biPlanes = 1, biBitCount = 24;
    uint32_t biCompression = 0, biSizeImage = 0, biXPelsPerMeter = 0, biYPelsPerMeter = 0, biClrUsed = 0,
             biClrImportant = 0;
};

struct BitmapFileHeader {
    uint16_t bfType = BITMAP_ID;
    uint32_t bfSize = 0;
    uint16_t bfReserved1 = 0, bfReserved2 = 0;
    uint32_t bfOffBits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
};
#pragma pack(pop)

export auto getTextureIndex(BlockID blockname, size_t face) -> TextureIndex;
export void LoadRGBImage(ImageRGB& tex, std::filesystem::path const& path);
export void LoadRGBAImage(ImageRGBA& tex, std::filesystem::path const& path, std::filesystem::path const& maskPath);

export auto LoadRGBTexture(std::filesystem::path const& path, bool bilinear = false) -> TextureID;
export auto
LoadRGBATexture(std::filesystem::path const& path, std::filesystem::path const& maskPath, bool bilinear = false)
    -> TextureID;
export auto LoadBlockTextureArray(std::filesystem::path const& path, std::filesystem::path const& maskPath)
    -> TextureID;
export void SaveRGBImage(std::filesystem::path const& path, ImageRGB const& image);

constexpr auto Indices = std::array{
    std::array{    TextureIndex::WHITE,      TextureIndex::WHITE,     TextureIndex::WHITE},
    std::array{     TextureIndex::ROCK,       TextureIndex::ROCK,      TextureIndex::ROCK},
    std::array{TextureIndex::GRASS_TOP, TextureIndex::GRASS_SIDE,      TextureIndex::DIRT},
    std::array{     TextureIndex::DIRT,       TextureIndex::DIRT,      TextureIndex::DIRT},
    std::array{    TextureIndex::STONE,      TextureIndex::STONE,     TextureIndex::STONE},
    std::array{    TextureIndex::PLANK,      TextureIndex::PLANK,     TextureIndex::PLANK},
    std::array{ TextureIndex::WOOD_TOP,  TextureIndex::WOOD_SIDE,  TextureIndex::WOOD_TOP},
    std::array{  TextureIndex::BEDROCK,    TextureIndex::BEDROCK,   TextureIndex::BEDROCK},
    std::array{     TextureIndex::LEAF,       TextureIndex::LEAF,      TextureIndex::LEAF},
    std::array{    TextureIndex::GLASS,      TextureIndex::GLASS,     TextureIndex::GLASS},
    std::array{    TextureIndex::WATER,      TextureIndex::WATER,     TextureIndex::WATER},
    std::array{     TextureIndex::LAVA,       TextureIndex::LAVA,      TextureIndex::LAVA},
    std::array{TextureIndex::GLOWSTONE,  TextureIndex::GLOWSTONE, TextureIndex::GLOWSTONE},
    std::array{     TextureIndex::SAND,       TextureIndex::SAND,      TextureIndex::SAND},
    std::array{   TextureIndex::CEMENT,     TextureIndex::CEMENT,    TextureIndex::CEMENT},
    std::array{      TextureIndex::ICE,        TextureIndex::ICE,       TextureIndex::ICE},
    std::array{     TextureIndex::COAL,       TextureIndex::COAL,      TextureIndex::COAL},
    std::array{     TextureIndex::IRON,       TextureIndex::IRON,      TextureIndex::IRON},
    std::array{      TextureIndex::TNT,        TextureIndex::TNT,       TextureIndex::TNT},
    std::array{TextureIndex::NULLBLOCK,  TextureIndex::NULLBLOCK, TextureIndex::NULLBLOCK},
};

TextureIndex getTextureIndex(BlockID blockname, size_t face) {
    auto i = static_cast<size_t>(blockname);
    auto j = static_cast<size_t>(face);
    return i < Indices.size() && j < Indices[i].size() ? Indices[i][j] : TextureIndex::NULLBLOCK;
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
