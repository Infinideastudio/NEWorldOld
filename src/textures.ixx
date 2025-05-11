module;

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#undef assert

export module textures;
import std;
import types;
import debug;
import blocks;
import globals;
import render;

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
export using render::ImageRGB;
export using render::ImageRGBA;

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

export auto LoadTexture(std::filesystem::path const& path, bool bilinear, bool flipped = true) -> TextureID {
    auto image = render::load_png_image(path);
    if (image) {
        if (flipped) {
            image->flip();
        }
        auto res = TextureID();
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
            static_cast<GLsizei>(image->width()),
            static_cast<GLsizei>(image->height()),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image->data()
        );
        glGenerateMipmap(GL_TEXTURE_2D);
        return res;
    }
    spdlog::error("failed to load texture {}: {}", path.string(), image.error());
    return TextureID();
}

export auto LoadBlockTextureArray(std::filesystem::path const& path, bool flipped = true) -> TextureID {
    auto image = render::load_png_image(path);
    if (image) {
        if (flipped) {
            image->flip();
        }
        auto res = TextureID();
        auto size = image->width();
        auto count = image->height() / size;
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
            image->data()
        );
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        return res;
    }
    spdlog::error("failed to load image {}: {}", path.string(), image.error());
    return TextureID();
}

export void SaveImage(std::filesystem::path const& path, ImageRGBA const& image) {
    auto res = render::save_png_image(path, image);
    if (res) {
        return;
    }
    spdlog::error("failed to save image {}: {}", path.string(), res.error());
}

}
