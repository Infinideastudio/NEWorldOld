#include "TextRenderer.h"
#include "Textures.h"
#include "Renderer.h"

namespace TextRenderer {
	int FontSize = 16;

	struct UnicodeChar {
		bool aval = false;
		TextureID tex = 0;
		float xpos = 0.0f, ypos = 0.0f;
		float width = 0.0f, height = 0.0f;
		float advance = 0.0f;
	};
	std::unordered_map<char32_t, UnicodeChar> chars;

	FT_Library library;
	FT_Face face;
	FT_GlyphSlot slot;
	int faceSize;
	float colr, colg, colb, cola;

	void initFont(bool reload) {
		if (!slot || reload) {
			if (FT_Init_FreeType(&library)) assert(false);
			if (FT_New_Face(library, "fonts/unicode.ttf", 0, &face)) assert(false);
			if (FT_Set_Pixel_Sizes(face, FontSize, FontSize)) assert(false);
			slot = face->glyph;
			faceSize = FontSize;
			chars.clear();
			/*
			unsigned int maxWidth = (face->bbox.xMax - face->bbox.xMin + 63) / 64;
			unsigned int maxHeight = (face->bbox.yMax - face->bbox.yMin + 63) / 64;
			std::cerr << maxWidth << ", " << maxHeight << std::endl;
			*/
		}
	}

	void setFontColor(float r, float g, float b, float a) {
		colr = r, colg = g, colb = b, cola = a;
	}

	UnicodeChar& loadChar(char32_t uc) {
		auto index = FT_Get_Char_Index(face, uc);
		FT_Load_Glyph(face, index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

		auto const& bitmap = slot->bitmap;
		assert(bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
		assert(bitmap.num_grays == 256);
		assert(bitmap.pitch >= 0);

		auto image = std::make_unique<uint8_t[]>(bitmap.rows * bitmap.width * 4); // Already aligned to 4 bytes
		for (unsigned int i = 0; i < bitmap.rows; i++) {
			for (unsigned int j = 0; j < bitmap.width; j++) {
				auto p = (bitmap.rows - 1 - i) * (bitmap.width * 4) + j * 4; // Assuming upside down bitmap data
				image[p + 0] = image[p + 1] = image[p + 2] = 255;
				image[p + 3] = bitmap.buffer[i * bitmap.pitch + j];
			}
		}

		auto& res = chars[uc];
		glGenTextures(1, &res.tex);
		glBindTexture(GL_TEXTURE_2D, res.tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.width, bitmap.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.get());

		auto const& metrics = slot->metrics;
		res.aval = true;
		res.width = static_cast<float>(metrics.width) / 64.0f;
		res.height = static_cast<float>(metrics.height) / 64.0f;
		res.advance = static_cast<float>(metrics.horiAdvance) / 64.0f;
		res.xpos = static_cast<float>(metrics.horiBearingX) / 64.0f;
		res.ypos = static_cast<float>(metrics.horiBearingY) / 64.0f;
		return res;
	}

	UnicodeChar& getChar(char32_t c) {
		auto it = chars.find(c);
		if (it != chars.end()) return it->second;
		return loadChar(c);
	}

	int getFontHeight() {
		float ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
		return static_cast<int>(std::round(ascender));
	}

	int getLineHeight() {
		float ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
		float descender = static_cast<float>(face->size->metrics.descender) / 64.0f;
		return static_cast<int>(std::round(ascender - descender));
		// float height = static_cast<float>(face->size->metrics.height) / 64.0f;
		// return static_cast<int>(std::round(height));
	}

	int getStringWidth(std::string const& s) {
		return getUnicodeStringWidth(UTF8Unicode(s));
	}

	int getUnicodeStringWidth(std::u32string const& s) {
		float res = 0.0f;
		for (size_t i = 0; i < s.size(); i++) {
			auto const& uc = getChar(s[i]);
			res += (i + 1 == s.size() ? static_cast<float>(uc.width) : uc.advance);
		}
		return static_cast<int>(std::round(res));
	}

	void renderString(int x, int y, std::string const& s) {
		renderUnicodeString(x, y, UTF8Unicode(s));
	}

	void renderUnicodeString(int x, int y, std::u32string const& s) {
		float dx = 0.0f, dy = 0.0f;
		for (size_t i = 0; i < s.size(); i++) {
			auto const& uc = getChar(s[i]);
			float xpos = static_cast<float>(x) + dx + uc.xpos;
			float ypos = static_cast<float>(y) + dy + getFontHeight() - uc.ypos;
			float width = uc.width;
			float height = uc.height;

			glBindTexture(GL_TEXTURE_2D, uc.tex);
			glBegin(GL_QUADS);
			glColor4f(0.5f, 0.5f, 0.5f, cola);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos + 1.0f, ypos + 1.0f);
			glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos + 1.0f, ypos + height + 1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + width + 1.0f, ypos + height + 1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + width + 1.0f, ypos + 1.0f);
			glColor4f(colr, colg, colb, cola);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(xpos, ypos);
			glTexCoord2f(0.0f, 0.0f); glVertex2f(xpos, ypos + height);
			glTexCoord2f(1.0f, 0.0f); glVertex2f(xpos + width, ypos + height);
			glTexCoord2f(1.0f, 1.0f); glVertex2f(xpos + width, ypos);
			glEnd();

			dx += uc.advance;
		}
	}
}
