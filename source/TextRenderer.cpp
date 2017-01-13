#include "TextRenderer.h"
#include "Textures.h"

namespace TextRenderer
{
    unsigned int gbe;
    unsigned int Font;
    int gloop;
    int ww, wh;
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    unsigned int unicodeTex[256];
    bool unicodeTexAval[256];
    bool useUnicodeASCIIFont;

    void BuildFont(int w, int h)
    {
        ww = w;
        wh = h;
        Font = Textures::LoadFontTexture("Textures\\Fonts\\ASCII.bmp");

        float cx, cy;
        gbe = glGenLists(256);
        glBindTexture(GL_TEXTURE_2D, Font);

        for (gloop = 0; gloop < 256; gloop++)
        {

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

    void setFontColor(float r_, float g_, float b_, float a_)
    {
        r = r_;
        g = g_;
        b = b_;
        a = a_;
    }

    int getStrWidth(string s)
    {
        int ret = 0;
        unsigned int i = 0;

        while(i<s.size())
        {
            if (s[i] >= 0 && s[i] <= 127)
            {
                i += 1;
                ret += 10;
            }
            else
            {
                i += 2;
                ret += 16;
            }
        }

        return ret;
    }

    void glprint(int x, int y, string glstring)
    {

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
    std::wstring c2w(string str) noexcept
    {
        size_t destlen = mbstowcs(nullptr, str.data(), 0);
        std::wstring val(destlen, L' ');
        mbstowcs(const_cast<wchar_t*>(val.c_str()), str.data(), destlen + 1);
        return val;
    }
    void renderString(int x, int y, string glstring)
    {
        double tx, ty, span = 0;
        TextureID ftex;

        auto wstr = c2w(glstring);

        glEnable(GL_TEXTURE_2D);
        glTranslated(x + 1, y + 1, 0);

        for (unsigned int k = 0; k < wstr.size(); k++)
        {
            int hbit = wstr[k] / 256, lbit = wstr[k] % 256;

            if (!useUnicodeASCIIFont && wstr[k] < 128)
            {
                ftex = Font;
            }
            else
            {
                if (!unicodeTexAval[hbit])
                {
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

            if (wstr[k] < 128)
            {
                span += 10;
            }
            else
            {
                span += 16;
            }
        }

        glTranslated(-x - 1, -y - 1, 0);
        glColor4f(1.0, 1.0, 1.0, 1.0);
    }
}