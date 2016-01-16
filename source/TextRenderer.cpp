#include "Definitions.h"
#include "TextRenderer.h"
#include "Textures.h"

namespace TextRenderer{
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
		Font = Textures::LoadFontTexture("Textures/Fonts/ASCII.bmp");

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
	}

	void setFontColor(float r_, float g_, float b_, float a_){
		r = r_;
		g = g_;
		b = b_;
		a = a_;
	}

	void MBToWC(const char* lpcszStr, wchar_t*& lpwszStr){
		wxString tmp = lpcszStr;
		lpwszStr = new wchar_t[tmp.size() + 2];
		wcscpy(lpwszStr, tmp.wc_str());
	}

	int getStrWidth(string s){
		int ret = 0;
		unsigned int i = 0;
		wchar_t* wstr = nullptr;
		MBToWC(s.c_str(), wstr);
		for (unsigned int k = 0, im = wstrlen(wstr); k < im; k++) {
			if (s[i] >= 0 && s[i] <= 127){
				i += 1;
				ret += 10;
			}
			else{
				i += 2;
				ret += 16;
			}
		}
		delete[] wstr;
		return ret;
	}

	void renderString(int x, int y, string glstring){
		double tx, ty, span = 0;
		TextureID ftex;

		wchar_t* wstr = nullptr;
		MBToWC(glstring.c_str(), wstr);

		glEnable(GL_TEXTURE_2D);
		glTranslated(x + 1, y + 1, 0);
		for (unsigned int k = 0; k < wstrlen(wstr); k++) {
			int hbit = wstr[k] / 256, lbit = wstr[k] % 256;
			if (!useUnicodeASCIIFont && wstr[k] < 128) ftex = Font;
			else {
				if (!unicodeTexAval[hbit]) {
					unicodeTex[hbit] = Textures::LoadFontTexture(hbit);
					unicodeTexAval[hbit] = true;
				}
				ftex = unicodeTex[hbit];
			}
			tx = (lbit % 16) * 0.0625;
			ty = 1 - (lbit / 16) * 0.0625;
			glBindTexture(GL_TEXTURE_2D, ftex);
			glBegin(GL_QUADS);
				glColor4f(0.5, 0.5, 0.5, a);
				glTexCoord2d(tx, ty);
				glVertex2i(1 + span, 1);
				glTexCoord2d(tx + 0.0625, ty);
				glVertex2i(17 + span, 1);
				glTexCoord2d(tx + 0.0625, ty - 0.0625);
				glVertex2i(17 + span, 17);
				glTexCoord2d(tx, ty - 0.0625);
				glVertex2i(1 + span, 17);

				glColor4f(r, g, b, a);
				glTexCoord2d(tx, ty);
				glVertex2i(0 + span, 0);
				glTexCoord2d(tx + 0.0625, ty);
				glVertex2i(16 + span, 0);
				glTexCoord2d(tx + 0.0625, ty - 0.0625);
				glVertex2i(16 + span, 16);
				glTexCoord2d(tx, ty - 0.0625);
				glVertex2i(0 + span, 16);
			glEnd();
			if (wstr[k] < 128) { span += 10; }
			else { span += 16; }
		}
		glTranslated(-x - 1, -y - 1, 0);
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