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
		Font = Textures::LoadFontTexture("Textures\\Fonts\\ASCII.bmp");

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

	void MBToWC(const char* lpcszStr, wchar_t*& lpwszStr, int dwSize){
		lpwszStr = (wchar_t*)malloc(dwSize);
		memset(lpwszStr, 0, dwSize);
		int iSize = (MByteToWChar(lpwszStr, lpcszStr, strlen(lpcszStr)) + 1)*sizeof(wchar_t);
		lpwszStr = (wchar_t*)realloc(lpwszStr, iSize);
	}

	int getStrWidth(string s){
		int ret = 0;
		unsigned int i = 0;
		wchar_t* wstr = nullptr;
		MBToWC(s.c_str(), wstr, 128);
		for (unsigned int k = 0; k < wstrlen(wstr); k++){
			if (s[i] >= 0 && s[i] <= 127){
				i += 1;
				ret += 10;
			}
			else{
				i += 2;
				ret += 16;
			}
		}
		free(wstr);
		return ret;
	}

	void glprint(int x, int y, string glstring){

		glBindTexture(GL_TEXTURE_2D, Font);
		glDisable(GL_DEPTH_TEST);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, ww, wh, 0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
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
		glMatrixMode(GL_PROJECTION);

		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		glEnable(GL_DEPTH_TEST);
		glColor4f(1.0, 1.0, 1.0, 1.0);
	}

	void renderString(int x, int y, string glstring){

		unsigned int i = 0;
		int uc;
		double tx, ty, span = 0;
		//Textures::TEXTURE_RGBA Tex;
		TextureID ftex;

		wchar_t* wstr = nullptr;
		MBToWC(glstring.c_str(), wstr, 128);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, ww, wh, 0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glEnable(GL_TEXTURE_2D);
		for (unsigned int k = 0; k < wstrlen(wstr); k++){
			glLoadIdentity();
			glColor4f(r, g, b, a);
			glTranslated(x + 1 + span, y + 1, 0);
			uc = wstr[k];
			if (!useUnicodeASCIIFont && glstring[i]>=0 && glstring[i] <= 127){
				glprint(x + 1 + (int)span, y + 1, string() + glstring[i]);
			}
			else{
				if (!unicodeTexAval[uc / 256]) {
					//printf("[Console][Event]");
					//printf("Loading unicode font texture #%d\n", uc / 256);
					std::stringstream ss;
					ss << "Textures\\Fonts\\unicode\\unicode_glyph_" << uc / 256 << ".bmp";
					ftex = Textures::LoadFontTexture(ss.str());
					unicodeTex[uc / 256] = ftex;
					unicodeTexAval[uc / 256] = true;
				}
				else{
					ftex = unicodeTex[uc / 256];
				}

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
			}
			if (glstring[i] >= 0 && glstring[i] <= 127){
				i += 1;
				span += 10;
			}
			else{
				i += 2;
				span += 16;
			}
		}

		glMatrixMode(GL_PROJECTION);

		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		glColor4f(1.0, 1.0, 1.0, 1.0);
		free(wstr);
	}
}