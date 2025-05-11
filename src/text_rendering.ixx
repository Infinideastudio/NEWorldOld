module;

#include <ft2build.h>
#include FT_FREETYPE_H
#include <glad/gl.h>
#undef assert

export module text_rendering;
import std;
import types;
import debug;
import math;
import globals;
import render;
import rendering;
import textures;

namespace TextRenderer {

constexpr auto BASE_FONT_SIZE = 16.0;
constexpr auto INITIAL_TEXTURE_DEPTH = 10uz;
constexpr auto TEXTURE_SIZE = 256uz;

struct UnicodeChar {
    Vec3f tc = 0.0f;
    float xpos = 0.0f, ypos = 0.0f;
    float width = 0.0f, height = 0.0f;
    float advance = 0.0f;
};
std::unordered_map<char32_t, UnicodeChar> chars;

FT_Library library;
FT_Face face;
FT_GlyphSlot slot;
render::ImageRGBA image = {};
render::Texture tex = {};
size_t curr_layer = 0uz;
size_t curr_row = 0uz;
size_t curr_col = 0uz;
size_t curr_row_height = 0uz;
int faceSize;
float colr, colg, colb, cola;

export void init_font(bool reload = false);
export void set_font_color(float r, float g, float b, float a);
export auto font_height() -> int;
export auto line_height() -> int;
export auto rendered_width(std::string_view s) -> int;
export auto rendered_width(std::u32string_view s) -> int;
export void render_string(int x, int y, std::string_view s);
export void render_string(int x, int y, std::u32string_view s);

export void init_font(bool reload) {
    if (!slot || reload) {
        // Initialize FreeType and load the font.
        faceSize = std::lround(BASE_FONT_SIZE * Stretch * FontScale);
        if (FT_Init_FreeType(&library))
            assert(false, "failed to initialize FreeType");
        if (FT_New_Face(library, "fonts/unicode.ttf", 0, &face))
            assert(false, "failed to load font");
        if (FT_Set_Pixel_Sizes(face, faceSize, faceSize))
            assert(false, "failed to set font size");
        slot = face->glyph;
        chars.clear();

        // Reset the image and texture.
        image = render::ImageRGBA(INITIAL_TEXTURE_DEPTH, TEXTURE_SIZE, TEXTURE_SIZE);
        tex = {};
        curr_layer = 0uz;
        curr_row = 0uz;
        curr_col = 0uz;
        curr_row_height = 0uz;
    }
}

export void set_font_color(float r, float g, float b, float a) {
    colr = r, colg = g, colb = b, cola = a;
}

auto loadChar(char32_t uc) -> UnicodeChar& {
    auto index = FT_Get_Char_Index(face, uc);
    FT_Load_Glyph(face, index, FT_LOAD_DEFAULT);
    FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

    auto const& bitmap = slot->bitmap;
    assert(
        bitmap.pixel_mode == FT_PIXEL_MODE_GRAY && bitmap.num_grays == 256 && bitmap.pitch >= 0,
        "unexpected bitmap format from font rendering"
    );
    assert(bitmap.rows <= TEXTURE_SIZE && bitmap.width <= TEXTURE_SIZE, "bitmap size exceeds maximum character size");

    // Allocate space for the character in the atlas image.
    // Leave 1px margin around each character.
    if (curr_col + bitmap.width + 1uz > TEXTURE_SIZE) {
        curr_row += curr_row_height;
        curr_col = 0uz;
        curr_row_height = 0uz;
    }
    if (curr_row + bitmap.rows + 1uz > TEXTURE_SIZE) {
        curr_layer++;
        curr_row = 0uz;
        curr_col = 0uz;
        curr_row_height = 0uz;
    }

    // Allocate new image layers when necessary.
    while (curr_layer >= image.depth()) {
        auto next = render::ImageRGBA(image.depth() * 2, image.height(), image.width());
        for (auto i = 0uz; i < image.depth(); i++) {
            next.fill(i, 0, 0, image, i);
        }
        image = std::move(next);
    }

    // Copy the bitmap into the atlas image.
    for (auto i = 0uz; i < bitmap.rows; i++) {
        for (auto j = 0uz; j < bitmap.width; j++) {
            image[curr_layer, curr_row + i, curr_col + j] =
                {255, 255, 255, bitmap.buffer[(bitmap.rows - 1 - i) * bitmap.pitch + j]};
        }
    }

    // Record and advance the current position in the atlas image.
    auto tc = Vec3f(curr_col, curr_row, curr_layer) / Vec3f(TEXTURE_SIZE, TEXTURE_SIZE, 1.0f);
    curr_row_height = std::max(curr_row_height, bitmap.rows + 1uz);
    curr_col += bitmap.width + 1uz;

    // Update the texture to match the new atlas image.
    if (tex.depth() != image.depth()) {
        tex = render::Texture::create(render::Texture::Format::RGBA, image.depth(), image.height(), image.width());
        tex.set_filter(false, false);
        tex.set_wrap(false);
        for (auto i = 0uz; i < image.depth(); i++) {
            tex.fill(i, 0, 0, image, i);
        }
    } else {
        tex.fill(curr_layer, 0, 0, image, curr_layer);
    }

    auto const& metrics = slot->metrics;
    auto& res = chars[uc];
    res.tc = tc;
    res.xpos = static_cast<float>(metrics.horiBearingX) / 64.0f;
    res.ypos = static_cast<float>(metrics.horiBearingY) / 64.0f;
    res.width = static_cast<float>(metrics.width) / 64.0f;
    res.height = static_cast<float>(metrics.height) / 64.0f;
    res.advance = static_cast<float>(metrics.horiAdvance) / 64.0f;
    return res;
}

auto getChar(char32_t c) -> UnicodeChar& {
    auto it = chars.find(c);
    if (it != chars.end())
        return it->second;
    return loadChar(c);
}

export auto font_height() -> int {
    float ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
    return static_cast<int>(std::round(ascender));
}

export auto line_height() -> int {
    float ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
    float descender = static_cast<float>(face->size->metrics.descender) / 64.0f;
    return static_cast<int>(std::round(ascender - descender));
    // float height = static_cast<float>(face->size->metrics.height) / 64.0f;
    // return static_cast<int>(std::round(height));
}

export auto rendered_width(std::string_view s) -> int {
    return rendered_width(utf8_unicode(s));
}

export auto rendered_width(std::u32string_view s) -> int {
    float res = 0.0f;
    for (size_t i = 0; i < s.size(); i++) {
        auto const& uc = getChar(s[i]);
        res += (i + 1 == s.size() ? static_cast<float>(uc.width) : uc.advance);
    }
    return static_cast<int>(std::round(res));
}

export void render_string(int x, int y, std::string_view s) {
    render_string(x, y, utf8_unicode(s));
}

export void render_string(int x, int y, std::u32string_view s) {
    namespace spec = render::attrib_layout::spec;
    auto v =
        render::AttribIndexBuilder<spec::Coord<spec::Vec2f>, spec::TexCoord<spec::Vec3f>, spec::Color<spec::Vec4u8>>();

    float dx = 0.0f, dy = 0.0f;
    for (auto i: s) {
        auto const& uc = getChar(i);
        auto tc = uc.tc;
        float xpos = static_cast<float>(x) + dx + uc.xpos;
        float ypos = static_cast<float>(y) + dy + static_cast<float>(font_height()) - uc.ypos;
        float width = uc.width;
        float height = uc.height;

        v.color({128, 128, 128, 255 * cola});
        v.tex_coord({tc.x(), tc.y() + height / TEXTURE_SIZE, tc.z()});
        v.coord({xpos + 1.0f, ypos + 1.0f});
        v.tex_coord({tc.x(), tc.y(), tc.z()});
        v.coord({xpos + 1.0f, ypos + height + 1.0f});
        v.tex_coord({tc.x() + width / TEXTURE_SIZE, tc.y(), tc.z()});
        v.coord({xpos + width + 1.0f, ypos + height + 1.0f});
        v.tex_coord({tc.x() + width / TEXTURE_SIZE, tc.y() + height / TEXTURE_SIZE, tc.z()});
        v.coord({xpos + width + 1.0f, ypos + 1.0f});
        v.end_primitive();

        v.color({255 * colr, 255 * colg, 255 * colb, 255 * cola});
        v.tex_coord({tc.x(), tc.y() + height / TEXTURE_SIZE, tc.z()});
        v.coord({xpos, ypos});
        v.tex_coord({tc.x(), tc.y(), tc.z()});
        v.coord({xpos, ypos + height});
        v.tex_coord({tc.x() + width / TEXTURE_SIZE, tc.y(), tc.z()});
        v.coord({xpos + width, ypos + height});
        v.tex_coord({tc.x() + width / TEXTURE_SIZE, tc.y() + height / TEXTURE_SIZE, tc.z()});
        v.coord({xpos + width, ypos});
        v.end_primitive();

        dx += uc.advance;
    }

    auto va = render::VertexArray::create(v, render::VertexArray::Primitive::TRIANGLE_FAN);
    if (va.first) {
        tex.bind(0);
        va.first.render();
    }
}

}
