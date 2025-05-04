module;

#include <cassert>
#include <glad/gl.h>

export module textures;
import std;
import types;
import blocks;
import globals;

export using TextureID = GLuint;

export TextureID SplashTexture;
export TextureID TitleTexture;
export std::array<TextureID, 6> UIBackgroundTextures;
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
    uint32_t bfSize = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    uint16_t bfReserved1 = 0, bfReserved2 = 0;
    uint32_t bfOffBits = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
};
#pragma pack(pop)

constexpr auto indices = std::array{
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

export auto getTextureIndex(blocks::Id blockname, size_t face) -> TextureIndex {
    auto i = blockname.get();
    auto j = face;
    return i < indices.size() && j < indices[i].size() ? indices[i][j] : TextureIndex::NULLBLOCK;
}

export auto loadRGBImage(std::filesystem::path const& path) -> ImageRGB {
    auto res = ImageRGB();
    auto bmp = std::ifstream(path, std::ios::binary | std::ios::in); // 位图文件（二进制）
    if (!bmp.is_open())
        assert(false);

    // 开始读取
    auto bfh = BitmapFileHeader(); // 各种关于文件的参数
    auto bih = BitmapInfoHeader(); // 各种关于位图的参数
    bmp.read(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    bmp.read(reinterpret_cast<char*>(&bih), sizeof(bih));
    res.sizeX = bih.biWidth;
    res.sizeY = bih.biHeight;
    res.buffer = std::make_unique<uint8_t[]>(res.sizeX * res.sizeY * 3);
    bmp.read(reinterpret_cast<char*>(res.buffer.get()), res.sizeX * res.sizeY * 3);

    // 转换BGR到RGB
    for (unsigned int i = 0; i < res.sizeX * res.sizeY; i++)
        std::swap(res.buffer[i * 3 + 0], res.buffer[i * 3 + 2]);

    return res;
}

export auto loadRGBAImage(std::filesystem::path const& path, std::filesystem::path const& maskPath) -> ImageRGBA {
    auto res = ImageRGBA();
    auto bmp = std::ifstream(path, std::ios::binary | std::ios::in);
    if (!bmp.is_open())
        assert(false);

    auto bfh = BitmapFileHeader();
    auto bih = BitmapInfoHeader();
    bmp.read(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    bmp.read(reinterpret_cast<char*>(&bih), sizeof(bih));
    res.sizeX = bih.biWidth;
    res.sizeY = bih.biHeight;
    auto rgb = std::make_unique<uint8_t[]>(res.sizeX * res.sizeY * 3);
    bmp.read(reinterpret_cast<char*>(rgb.get()), res.sizeX * res.sizeY * 3);

    res.buffer = std::make_unique<uint8_t[]>(res.sizeX * res.sizeY * 4);
    for (unsigned int i = 0; i < res.sizeX * res.sizeY; i++) {
        res.buffer[i * 4 + 0] = rgb[i * 3 + 2];
        res.buffer[i * 4 + 1] = rgb[i * 3 + 1];
        res.buffer[i * 4 + 2] = rgb[i * 3 + 0];
        res.buffer[i * 4 + 3] = 255;
    }

    if (!maskPath.empty()) {
        auto mask = std::ifstream(maskPath, std::ios::binary | std::ios::in);
        if (!mask.is_open())
            assert(false);

        auto mbfh = BitmapFileHeader();
        auto mbih = BitmapInfoHeader();
        mask.read(reinterpret_cast<char*>(&mbfh), sizeof(mbfh));
        mask.read(reinterpret_cast<char*>(&mbih), sizeof(mbih));
        auto alpha = std::make_unique<uint8_t[]>(res.sizeX * res.sizeY * 3);
        mask.read(reinterpret_cast<char*>(alpha.get()), res.sizeX * res.sizeY * 3);

        for (unsigned int i = 0; i < res.sizeX * res.sizeY; i++)
            res.buffer[i * 4 + 3] -= alpha[i * 3 + 0];
    }

    return res;
}

export auto loadRGBTexture(std::filesystem::path const& path, bool bilinear) -> TextureID {
    auto res = TextureID();
    auto image = loadRGBImage(path);
    glGenTextures(1, &res);
    glBindTexture(GL_TEXTURE_2D, res);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB8,
        static_cast<GLsizei>(image.sizeX),
        static_cast<GLsizei>(image.sizeY),
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        image.buffer.get()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    return res;
}

export auto loadRGBATexture(std::filesystem::path const& path, std::filesystem::path const& maskPath, bool bilinear)
    -> TextureID {
    auto res = TextureID();
    auto image = loadRGBAImage(path, maskPath);
    glGenTextures(1, &res);
    glBindTexture(GL_TEXTURE_2D, res);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(image.sizeX),
        static_cast<GLsizei>(image.sizeY),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.buffer.get()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    return res;
}

export auto loadBlockTextureArray(std::filesystem::path const& path, std::filesystem::path const& maskPath)
    -> TextureID {
    auto res = TextureID();
    auto image = loadRGBAImage(path, maskPath);
    auto size = image.sizeX;
    auto count = image.sizeY / size;
    glGenTextures(1, &res);
    glBindTexture(GL_TEXTURE_2D_ARRAY, res);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(size),
        static_cast<GLsizei>(size),
        static_cast<GLsizei>(count),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.buffer.get()
    );
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    return res;
}

export void SaveRGBImage(std::filesystem::path const& path, ImageRGB const& image) {
    auto bfh = BitmapFileHeader();
    auto bih = BitmapInfoHeader();
    bfh.bfSize += image.sizeX * image.sizeY * 3;
    bih.biWidth = image.sizeX;
    bih.biHeight = image.sizeY;
    bih.biSizeImage = image.sizeX * image.sizeY * 3;
    for (unsigned int i = 0; i != image.sizeX * image.sizeY; i++)
        std::swap(image.buffer[i * 3 + 0], image.buffer[i * 3 + 2]);
    auto ofs = std::ofstream(path, std::ios::out | std::ios::binary);
    ofs.write(reinterpret_cast<char*>(&bfh), sizeof(bfh));
    ofs.write(reinterpret_cast<char*>(&bih), sizeof(bih));
    ofs.write(reinterpret_cast<char*>(image.buffer.get()), image.sizeX * image.sizeY * 3);
}
}
