#pragma once

#include "Definitions.h"

namespace TextRenderer {
    extern unsigned int gbe;
    extern unsigned int Font;
    extern int gloop;
    extern int ww, wh;
    extern float r, g, b, a;
    extern unsigned int unicodeTex[256];
    extern bool unicodeTexAval[256];
    extern bool useUnicodeASCIIFont;

    void BuildFont(int w, int h);

    void resize();

    void setFontColor(float r, float g, float b, float a);

    void loadchar(unsigned int uc);

    int getStrWidth(std::string s);

    void renderString(int x, int y, std::string glstring);

    void renderASCIIString(int x, int y, std::string glstring);
}