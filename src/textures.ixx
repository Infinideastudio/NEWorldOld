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

export render::Texture SplashTexture;
export std::shared_ptr<render::Texture> TitleTexture;
export std::shared_ptr<render::Texture> UIBackgroundTextures;
export render::Texture SelectedTexture;
export render::Texture UnselectedTexture;
export render::Texture BlockTextureArray;
export render::Texture NormalTextureArray;
export render::Texture NoiseTextureArray;

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

export auto LoadTexture(std::filesystem::path const& path, bool bilinear) -> render::Texture {
    auto image = render::load_png_image(path);
    if (image) {
        auto res = render::Texture::create(render::Texture::Format::SRGBA, image->width(), image->height());
        res.fill(0, 0, 0, *image);
        res.set_filter(bilinear, true);
        res.generate_mipmaps();
        return std::move(res);
    }
    spdlog::error("failed to load image {}: {}", path.string(), image.error());
    return {};
}

export auto LoadBlockTextureArray(std::filesystem::path const& path) -> render::Texture {
    auto image = render::load_png_image(path);
    if (image) {
        auto size = image->width();
        auto count = image->height() / size;
        image->reshape(size, size, count);
        auto res = render::Texture::create(render::Texture::Format::SRGBA, size, size, count);
        for (auto i = 0uz; i < count; ++i) {
            res.fill(0, 0, i, *image, i);
        }
        res.set_wrap(true);
        res.set_filter(false, true);
        res.generate_mipmaps();
        return std::move(res);
    }
    spdlog::error("failed to load image {}: {}", path.string(), image.error());
    return {};
}

export auto LoadNormalTextureArray(std::filesystem::path const& path) -> render::Texture {
    auto image = render::load_png_image(path);
    if (image) {
        auto size = image->width();
        auto count = image->height() / size;
        image->reshape(size, size, count);
        auto res = render::Texture::create(render::Texture::Format::RGBA, size, size, count);
        for (auto i = 0uz; i < count; ++i) {
            res.fill(0, 0, i, *image, i);
        }
        res.set_wrap(true);
        res.set_filter(false, true);
        res.generate_mipmaps();
        return std::move(res);
    }
    spdlog::error("failed to load image {}: {}", path.string(), image.error());
    return {};
}

export auto LoadNoiseTextureArray(std::filesystem::path const& path) -> render::Texture {
    auto image = render::load_png_image(path);
    if (image) {
        auto res = render::Texture::create(render::Texture::Format::RGBA, image->width(), image->height());
        res.fill(0, 0, 0, *image);
        res.set_wrap(true);
        res.set_filter(true, false);
        return std::move(res);
    }
    spdlog::error("failed to load image {}: {}", path.string(), image.error());
    return {};
}

export void SaveImage(std::filesystem::path const& path, ImageRGBA const& image) {
    auto res = render::save_png_image(path, image);
    if (res) {
        return;
    }
    spdlog::error("failed to save image {}: {}", path.string(), res.error());
}

}
