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

	void BuildFont(int w, int h){
		ww = w;
		wh = h;
		Font = Textures::LoadFontTexture("Fonts/ASCII.bmp");

		float cx, cy;
		gbe = glGenLists(256);
		glBindTexture(GL_TEXTURE_2D, Font);
		for (gloop = 0; gloop < 256; gloop++){

			cx = (float)(gloop % 16) / 16.0f;
			cy = (float)(gloop / 16) / 16.0f;

			glNewList(gbe + gloop, GL_COMPILE);
			glBegin(GL_QUADS);
			glTexCoord2f(cx, 1.0f - cy);
			glVertex2i(0, 0);
			glTexCoord2f(cx + 0.0625f, 1.0f - cy);
			glVertex2i(16, 0);
			glTexCoord2f(cx + 0.0625f, 1.0f - cy - 0.0625f);
			glVertex2i(16, 16);
			glTexCoord2f(cx, 1.0f - cy - 0.0625f);
			glVertex2i(0, 16);
			glEnd();
			glTranslated(10.0, 0.0, 0.0);
			glEndList();

		}

		if (FT_Init_FreeType(&library)) {
			//assert(false);
		}
		if (FT_New_Face(library, "Fonts/Font.ttf", 0, &fontface)) {
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

	void MBToWC(const char* lpcszStr, wchar_t*& lpwszStr, int dwSize){
		lpwszStr = (wchar_t*)malloc(dwSize);
		memset(lpwszStr, 0, dwSize);
		int iSize = (MByteToWChar(lpwszStr, lpcszStr, dwSize, (int) strlen(lpcszStr)) + 1)*sizeof(wchar_t);
		lpwszStr = (wchar_t*)realloc(lpwszStr, iSize);
	}

	int getStrWidth(string s){
		UnicodeChar c;
		int uc, res = 0;
		unsigned int i = 0;
		wchar_t* wstr = nullptr;
		MBToWC(s.c_str(), wstr, 128);
		for (unsigned int k = 0; k < wstrlen(wstr); k++){
			if (s[i] >= 0 && s[i] <= 127) i++; else i += 2;
			uc = wstr[k];
			c = chars[uc];
			if (!c.aval) {
				loadchar(uc);
				c = chars[uc];
			}
			res += c.advance;
		}
		free(wstr);
		return res;
	}

	void renderString(int x, int y, string glstring){
		UnicodeChar c;
		unsigned int i = 0;
		int uc;
		int span = 0;
		wchar_t* wstr = nullptr;
		MBToWC(glstring.c_str(), wstr, 128);

		glEnable(GL_TEXTURE_2D);
		for (unsigned int k = 0; k < wstrlen(wstr); k++) {
			//glLoadIdentity();
			//glColor4f(r, g, b, a);
			/*
			if (!useUnicodeASCIIFont && glstring[i] >= 0 && glstring[i] <= 127) ftex = Font;
			else {
				if (!unicodeTexAval[uc / 256]) {
					std::stringstream ss;
					ss << "Textures/Fonts/unicode/unicode_glyph_" << uc / 256 << ".bmp";
					ftex = Textures::LoadFontTexture(ss.str());
					unicodeTex[uc / 256] = ftex;
					unicodeTexAval[uc / 256] = true;
				}
				else ftex = unicodeTex[uc / 256];
			}
			*/

			uc = wstr[k];
			c = chars[uc];
			if (!c.aval) {
				loadchar(uc);
				c = chars[uc];
			}

			glBindTexture(GL_TEXTURE_2D, c.tex);

			UITrans(x + 1 + span, y + 1);
			glColor4f(0.5, 0.5, 0.5, a);
			glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0);
			UIVertex(c.xpos, 15 - c.ypos);
			glTexCoord2d(c.width / 32.0, 0.0);
			UIVertex(c.xpos + c.width, 15- c.ypos);
			glTexCoord2d(c.width / 32.0, c.height / 32.0);
			UIVertex(c.xpos + c.width, 15 + c.height - c.ypos);
			glTexCoord2d(0.0, c.height / 32.0);
			UIVertex(c.xpos, 15 + c.height - c.ypos);
			glEnd();

			UITrans(-1, -1);
			glColor4f(r, g, b, a);
			glBegin(GL_QUADS);
			glTexCoord2d(0.0, 0.0);
			UIVertex(c.xpos, 15 - c.ypos);
			glTexCoord2d(c.width / 32.0, 0.0);
			UIVertex(c.xpos + c.width, 15 - c.ypos);
			glTexCoord2d(c.width / 32.0, c.height / 32.0);
			UIVertex(c.xpos + c.width, 15 + c.height - c.ypos);
			glTexCoord2d(0.0, c.height / 32.0);
			UIVertex(c.xpos, 15 + c.height - c.ypos);
			glEnd();

			/*
			tx = ((uc % 256) % 16) / 16.0;
			ty = 1 - ((uc % 256) / 16) / 16.0;
			glBindTexture(GL_TEXTURE_2D, ftex);
			glBegin(GL_QUADS);
				glColor4f(0.5, 0.5, 0.5, a);
				glTexCoord2d(tx, ty);
				glVertex2i(1, 1);
				glTexCoord2d(tx + 0.0625, ty);
				glVertex2i(17, 1);
				glTexCoord2d(tx + 0.0625, ty - 0.0625);
				glVertex2i(17, 17);
				glTexCoord2d(tx, ty - 0.0625);
				glVertex2i(1, 17);
				glColor4f(r, g, b, a);
				glTexCoord2d(tx, ty);
				glVertex2i(0, 0);
				glTexCoord2d(tx + 0.0625, ty);
				glVertex2i(16, 0);
				glTexCoord2d(tx + 0.0625, ty - 0.0625);
				glVertex2i(16, 16);
				glTexCoord2d(tx, ty - 0.0625);
				glVertex2i(0, 16);
			glEnd();
			glTranslated(-x - 1 - span, -y - 1, 0);
			*/

			UITrans(-x - span, -y);
			span += c.advance;

			if (glstring[i] >= 0 && glstring[i] <= 127) i++;
			else i += 2;
		}
		/*
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		*/

		glColor4f(1.0, 1.0, 1.0, 1.0);
		free(wstr);
	}

	void renderASCIIString(int x, int y, string glstring) {
		//glBindTexture(GL_TEXTURE_2D, Font);
		glPushMatrix();
		glLoadIdentity();
		glColor4f(0.5, 0.5, 0.5, a);
		glTranslated(x + 1, y + 1, 0);
		glListBase(gbe);
		glCallLists((GLsizei)glstring.length(), GL_UNSIGNED_BYTE, glstring.c_str());
		glLoadIdentity();
		glColor4f(r, g, b, a);
		glTranslated(x, y, 0);
		glListBase(gbe);
		glCallLists((GLsizei)glstring.length(), GL_UNSIGNED_BYTE, glstring.c_str());
		glPopMatrix();
	}

}