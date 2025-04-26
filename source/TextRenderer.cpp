#include "TextRenderer.h"
#include "Textures.h"

namespace TextRenderer {
	FT_Library library;
	FT_Face fontface;
	FT_GlyphSlot slot;
	UnicodeChar chars[65536];
	unsigned int gbe;
	unsigned int Font;
	int gloop;
	int ww, wh;
	float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
	unsigned int unicodeTex[256];
	bool unicodeTexAval[256];
	bool useUnicodeASCIIFont;

	void BuildFont(int w, int h) {
		if (Font != 0) return;

		ww = w;
		wh = h;
		Font = Textures::LoadFontTexture("fonts/ascii.bmp");

		float cx, cy;
		gbe = glGenLists(256);
		for (gloop = 0; gloop < 256; gloop++){

			cx = (float)(gloop % 16) / 16.0f;
			cy = (float)(gloop / 16) / 16.0f;

			glNewList(gbe + gloop, GL_COMPILE);
			glBegin(GL_QUADS);
			glTexCoord2f(cx, 1.0f - cy); glVertex2i(0, 0);
			glTexCoord2f(cx, 1.0f - cy - 0.0625f); glVertex2i(0, 16);
			glTexCoord2f(cx + 0.0625f, 1.0f - cy - 0.0625f); glVertex2i(16, 16);
			glTexCoord2f(cx + 0.0625f, 1.0f - cy); glVertex2i(16, 0);
			glEnd();
			glTranslated(10.0, 0.0, 0.0);
			glEndList();

		}

		if (FT_Init_FreeType(&library)) {
			//assert(false);
		}
		if (FT_New_Face(library, "fonts/unicode.ttf", 0, &fontface)) {
			//assert(false);
		}
		if (FT_Set_Pixel_Sizes(fontface, 16, 16)) {
			//assert(false);
		}
		slot = fontface->glyph;

	}

	void setFontColor(float r_, float g_, float b_, float a_){
		r = r_; g = g_; b = b_; a = a_;
	}

	void loadchar(unsigned int uc) {
		FT_Bitmap* bitmap;
		unsigned int index;
		uint8_t *Tex, *Texsrc;

		index = FT_Get_Char_Index(fontface, uc);
		FT_Load_Glyph(fontface, index, FT_LOAD_DEFAULT);
		FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
		bitmap = &(slot->bitmap);
		Texsrc = bitmap->buffer;
		Tex = new uint8_t[32 * 32 * 4];
		memset(Tex, 0, 32 * 32 * 4 * sizeof(uint8_t));
		for (unsigned int i = 0; i < bitmap->rows; i++) {
			for (unsigned int j = 0; j < bitmap->width; j++) {
				Tex[(i * 32 + j) * 4 + 0] = Tex[(i * 32 + j) * 4 + 1] = Tex[(i * 32 + j) * 4 + 2] = 255U;
				Tex[(i * 32 + j) * 4 + 3] = *Texsrc; Texsrc++;
			}
		}
		glGenTextures(1, &chars[uc].tex);
		glBindTexture(GL_TEXTURE_2D, chars[uc].tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, Tex);
		delete[] Tex;
		chars[uc].aval = true;
		chars[uc].width = bitmap->width;
		chars[uc].height = bitmap->rows;
		chars[uc].advance = slot->advance.x >> 6;
		chars[uc].xpos = slot->bitmap_left;
		chars[uc].ypos = slot->bitmap_top;
	}

	std::wstring MBToWC(std::string const& multiByte, int maxCount) {
		auto buffer = std::make_unique<wchar_t[]>(maxCount);
		std::fill(buffer.get(), buffer.get() + maxCount, static_cast<wchar_t>(0));
		auto actualCount = MByteToWChar(buffer.get(), multiByte.c_str(), maxCount, -1);
		return std::wstring(buffer.get(), buffer.get() + actualCount);
	}

	int getStrWidth(string s) {
		UnicodeChar c;
		int res = 0;
		auto wstr = MBToWC(s, 128);
		for (auto uc : wstr){
			c = chars[uc];
			if (!c.aval) {
				loadchar(uc);
				c = chars[uc];
			}
			res += c.advance;
		}
		return res;
	}

	void renderString(int x, int y, string s) {
		UnicodeChar c;
		int span = 0;
		auto wstr = MBToWC(s, 128);

		for (auto uc: wstr) {
			c = chars[uc];
			if (!c.aval) {
				loadchar(uc);
				c = chars[uc];
			}

			glBindTexture(GL_TEXTURE_2D, c.tex);

			UITrans(x + 1 + span, y + 1);
			glColor4f(0.5, 0.5, 0.5, a);
			glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0); UIVertex(c.xpos, 15 - c.ypos);
			glTexCoord2d(0.0, c.height / 32.0); UIVertex(c.xpos, 15 + c.height - c.ypos);
			glTexCoord2d(c.width / 32.0, c.height / 32.0); UIVertex(c.xpos + c.width, 15 + c.height - c.ypos);
			glTexCoord2d(c.width / 32.0, 0.0); UIVertex(c.xpos + c.width, 15 - c.ypos);
			glEnd();

			UITrans(-1, -1);
			glColor4f(r, g, b, a);
			glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0); UIVertex(c.xpos, 15 - c.ypos);
			glTexCoord2d(0.0, c.height / 32.0); UIVertex(c.xpos, 15 + c.height - c.ypos);
			glTexCoord2d(c.width / 32.0, c.height / 32.0); UIVertex(c.xpos + c.width, 15 + c.height - c.ypos);
			glTexCoord2d(c.width / 32.0, 0.0); UIVertex(c.xpos + c.width, 15 - c.ypos);
			glEnd();

			UITrans(-x - span, -y);
			span += c.advance;
		}
	}

	void renderASCIIString(int x, int y, string s) {
		glBindTexture(GL_TEXTURE_2D, Font);
		glPushMatrix();
		glLoadIdentity();
		glColor4f(0.5, 0.5, 0.5, a);
		glTranslated(x + 1, y + 1, 0);
		glListBase(gbe);
		glCallLists((GLsizei)s.length(), GL_UNSIGNED_BYTE, s.c_str());
		glLoadIdentity();
		glColor4f(r, g, b, a);
		glTranslated(x, y, 0);
		glListBase(gbe);
		glCallLists((GLsizei)s.length(), GL_UNSIGNED_BYTE, s.c_str());
		glPopMatrix();
	}

}
