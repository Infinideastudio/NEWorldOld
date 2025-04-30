#pragma once
#include "Definitions.h"

namespace TextRenderer {
    extern int FontSize;

    void initFont(bool reload = false);
    void setFontColor(float r, float g, float b, float a);
    int getFontHeight();
    int getLineHeight();
    int getStringWidth(std::string const& s);
    int getStringWidth(std::u32string const& s);
    void renderString(int x, int y, std::string const& s);
    void renderString(int x, int y, std::u32string const& s);
}
