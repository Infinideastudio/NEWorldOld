#pragma once
#include "Definitions.h"

namespace TextRenderer {
	struct UnicodeChar {
		bool aval = false;
		TextureID tex = 0;
		int xpos = 0, ypos = 0;
		int width = 0, height = 0, advance = 0;
	};

	extern FT_Library library;
	extern FT_Face fontface;
	extern FT_GlyphSlot slot;
	extern UnicodeChar chars[65536];

	extern unsigned int gbe;
	extern unsigned int Font;
	extern int gloop;
	extern int ww, wh;
	extern float r, g, b, a;
	extern unsigned int unicodeTex[256];
	extern bool unicodeTexAval[256];
	extern bool useUnicodeASCIIFont;

	void BuildFont(int w, int h);
	void setFontColor(float r, float g, float b, float a);
	void loadchar(unsigned int uc);
	int getStrWidth(string s);
	void renderString(int x, int y, string s);
	void renderASCIIString(int x, int y, string s);
}