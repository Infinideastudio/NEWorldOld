#include "GUI.h"
#include "TextRenderer.h"
extern string inputstr;

//图形界面系统。。。正宗OOP！！！
namespace gui
{
    float linewidth = 1.0f;
    float linealpha = 0.9f;
    float FgR = 0.2f;
    float FgG = 0.2f;
    float FgB = 0.2f;
    float FgA = 0.6f;
    float BgR = 0.2f;
    float BgG = 0.2f;
    float BgB = 0.2f;
    float BgA = 0.3f;

    unsigned int transitionList;
    unsigned int lastdisplaylist;
    double transitionTimer;
    bool transitionForward;

    void clearTransition()
    {
        if (transitionList != 0)
        {
            glDeleteLists(transitionList, 1);
            transitionList = 0;
        }

        if (lastdisplaylist != 0)
        {
            glDeleteLists(lastdisplaylist, 1);
            lastdisplaylist = 0;
        }
    }

    void screenBlur()
    {
        /*
        int w = windowwidth; //Width
        int h = windowheight; //Height
        int r = 8; //范围
        int psw = w + 2 * r, psh = h + 2 * r;
        ubyte *scr; //屏幕像素缓存
        int *cps; //列前缀和
        int *rps; //行前缀和
        int* sum;
        scr = new ubyte[w*h * 3];
        cps = new int[psw*psh * 3];
        rps = new int[psw*psh * 3];
        sum = new int[w*h * 3];
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, scr);
        //处理前缀和
        for (int col = 0; col < 3; col++) {
            for (int x = 0; x < psw; x++) {
                for (int y = 0; y < psh; y++) {
                    int scrx = x - r, scry = y - r;
                    if (scrx < 0) scrx = 0; if (scrx >= w) scrx = w - 1;
                    if (scry < 0) scry = 0; if (scry >= h) scry = h - 1;
                    rps[(y*psw + x) * 3 + col] = cps[(y*psw + x) * 3 + col] = scr[(scry*w + scrx) * 3 + col];
                    if (x != 0) rps[(y*psw + x) * 3 + col] += rps[(y*psw + (x - 1)) * 3 + col];
                    if (y != 0) cps[(y*psw + x) * 3 + col] += cps[((y - 1)*psw + x) * 3 + col];
                }
            }
        }
        //模糊计算
        int cursum; //当前颜色之和
        for (int col = 0; col < 3; col++) {
            cursum = 0;
            for (int y = 0; y <= 2 * r; y++) cursum += rps[(y*psw + 2 * r) * 3 + col];
            for (int x = 0; x < w; x++) {
                int psx = x + r;
                if (x != 0) {
                    cursum = sum[(x - 1) * 3 + col];
                    cursum -= cps[(2 * r*psw + psx - r - 1) * 3 + col];
                    cursum += cps[(2 * r*psw + psx + r) * 3 + col];
                }
                for (int y = 0; y < h; y++) {
                    int psy = y + r;
                    if (y != 0) {
                        cursum -= rps[((psy - r - 1)*psw + psx + r) * 3 + col];
                        if (x != 0) cursum += rps[((psy - r - 1)*psw + psx - r - 1) * 3 + col];
                        cursum += rps[((psy + r)*psw + psx + r) * 3 + col];
                        if (x != 0) cursum -= rps[((psy + r)*psw + psx - r - 1) * 3 + col];
                    }
                    sum[(y*w + x) * 3 + col] = cursum;
                    scr[(y*w + x) * 3 + col] = (ubyte)(cursum / ((2 * r + 1)*(2 * r + 1)));
                }
            }
        }
        glDrawPixels(w, h, GL_RGB, GL_UNSIGNED_BYTE, scr);
        delete[] scr;
        delete[] cps;
        delete[] rps;
        delete[] sum;
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        */
        static int szl = 0, rl = 0;
        static float *mat = nullptr;
        static ubyte *scr; //屏幕像素缓存

        int w = windowwidth; //Width
        int h = windowheight; //Height
        int r = 2; //范围
        int sz = 1;
        float scale = 2;
        TextureID bgTex;

        while (sz < w || sz < h)
        {
            sz *= 2;
        }

        if (sz != szl)
        {
            szl = sz;
            delete[] scr;
            scr = new ubyte[sz * sz * 3];
        }

        if (rl != r)
        {
            if (mat != nullptr)
            {
                delete[] mat;
            }

            int size = r * 2 + 1;
            int size2 = size * size;
            float sum = 0.0f;
            int index = 0;
            mat = new float[size2];

            for (int x = -r; x <= r; x++)
            {
                for (int y = -r; y <= r; y++)
                {
                    float val = 1.0f / (float)(abs(x) + abs(y) + 1);
                    mat[index++] = val;
                    sum += val;
                }
            }

            sum = 1.0f / sum;

            for (int i = 0; i < size2; i++)
            {
                mat[i] *= sum;
            }
        }

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glReadPixels(0, 0, sz, sz, GL_RGB, GL_UNSIGNED_BYTE, scr);
        //glColorMask(true, true, true, false);

        glGenTextures(1, &bgTex);
        glBindTexture(GL_TEXTURE_2D, bgTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sz, sz, 0, GL_RGB, GL_UNSIGNED_BYTE, scr);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int x = -r; x <= r; x++)
        {
            for (int y = -r; y <= r; y++)
            {
                float d = mat[(x + r) * (r * 2 + 1) + y + r];
                glColor4f(1.0f, 1.0f, 1.0f, d);
                glBegin(GL_QUADS);
                glTexCoord2f(0.0f, (float)h / sz);
                glVertex2f(x * scale, y * scale);
                glTexCoord2f((float)w / sz, (float)h / sz);
                glVertex2f(w + x * scale, y * scale);
                glTexCoord2f((float)w / sz, 0.0f);
                glVertex2f(w + x * scale, h + y * scale);
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(x * scale, h + y * scale);
                glEnd();
            }
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDeleteTextures(1, &bgTex);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        //glColorMask(true, true, true, true);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void drawBackground()
    {
        static double startTimer = timer();
        double elapsed = timer() - startTimer;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(90.0, (double)windowwidth / windowheight, 0.1, 10.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotated(elapsed * 4.0, 0.1, 1.0, 0.1);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        //Begin to draw a cube
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[0]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[1]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, 1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[2]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, 1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[3]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[4]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(-1.0f, -1.0f, 1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 1.0f);
        glEnd();
        glBindTexture(GL_TEXTURE_2D, tex_mainmenu[5]);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, 1.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(1.0f, 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(-1.0f, 1.0f, 1.0f);
        glEnd();
    }

    void controls::updatepos()
    {
        xmin = (int)(windowwidth * _xmin_b) + _xmin_r;
        ymin = (int)(windowheight * _ymin_b) + _ymin_r;
        xmax = (int)(windowwidth * _xmax_b) + _xmax_r;
        ymax = (int)(windowheight * _ymax_b) + _ymax_r;
    }

    void controls::resize(int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        _xmin_r = xi_r;
        _xmax_r = xa_r;
        _ymin_r = yi_r;
        _ymax_r = ya_r;
        _xmin_b = xi_b;
        _xmax_b = xa_b;
        _ymin_b = yi_b;
        _ymax_b = ya_b;
    }

    void label::update()
    {
        //更新标签状态
        if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)               //鼠标悬停
        {
            mouseon = true;
        }
        else
        {
            mouseon = false;
        }

        if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        {
            parent->focusid = id;    //焦点在此
        }

        focused = parent->focusid == id;   //焦点
    }

    void label::render()
    {
        //渲染标签
        float fcR, fcG, fcB, fcA;
        fcR = FgR;
        fcG = FgG;
        fcB = FgB;
        fcA = FgA;

        if (mouseon)
        {
            fcR = FgR * 1.2f;
            fcG = FgG * 1.2f;
            fcB = FgB * 1.2f;
            fcA = FgA * 0.8f;
        }

        if (focused)                                                   //Focus
        {
            glDisable(GL_TEXTURE_2D);
            glColor4f(FgR * 0.6f, FgG * 0.6f, FgB * 0.6f, linealpha);
            glLineWidth(linewidth);
            //glBegin(GL_POINTS)
            //glVertex2i(xmin - 1, ymin)
            //glEnd()
            glBegin(GL_LINE_LOOP);
            glVertex2i(xmin, ymin);
            glVertex2i(xmin, ymax);
            glVertex2i(xmax, ymax);
            glVertex2i(xmax, ymin);
            glEnd();
        }

        glEnable(GL_TEXTURE_2D);
        TextRenderer::setFontColor(fcR, fcG, fcB, fcA);
        TextRenderer::renderString(xmin, ymin, text);
    }

    void button::update()
    {
        if (!enabled)
        {
            mouseon = false, focused = false, pressed = false, clicked = false;
            return;
        }

        //更新按钮状态
        if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        {
            mouseon = true;
        }
        else
        {
            mouseon = false;
        }

        if ((parent->mb == 1 && mouseon || parent->enterp) && focused)
        {
            pressed = true;
        }
        else
        {
            pressed = false;
        }

        if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        {
            parent->focusid = id;
        }

        if (parent->focusid == id)
        {
            focused = true;
        }
        else
        {
            focused = false;
        }

        clicked = (parent->mb == 0 && parent->mbl == 1 && mouseon || parent->enterpl && parent->enterp == false) && focused;
        //clicked = lp&&!pressed

    }

    void button::render()
    {

        //渲染按钮
        float fcR, fcG, fcB, fcA;
        fcR = FgR;
        fcG = FgG;
        fcB = FgB;
        fcA = FgA;

        if (mouseon)
        {
            fcR = FgR * 1.2f;
            fcG = FgG * 1.2f;
            fcB = FgB * 1.2f;
            fcA = FgA * 0.8f;
        }

        if (pressed)
        {
            fcR = FgR * 0.8f;
            fcG = FgG * 0.8f;
            fcB = FgB * 0.8f;
            fcA = FgA * 1.5f;
        }

        if (!enabled)
        {
            fcR = FgR * 0.5f;
            fcG = FgG * 0.5f;
            fcB = FgB * 0.5f;
            fcA = FgA * 0.3f;
        }

        glColor4f(fcR, fcG, fcB, fcA);

        glDisable(GL_TEXTURE_2D);    //Button
        glBegin(GL_QUADS);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();
        glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);

        if (!enabled)
        {
            glColor4f(0.5f, 0.5f, 0.5f, linealpha);
        }

        glLineWidth(linewidth);
        glBegin(GL_LINE_LOOP);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();

        glBegin(GL_LINE_LOOP);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmin + 1, ymin + 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmin + 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmax - 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmax - 1, ymin + 1);

        glEnd();

        glEnable(GL_TEXTURE_2D);
        TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);

        if (!enabled)
        {
            TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
        }

        TextRenderer::renderString((xmin + xmax - TextRenderer::getStrWidth(text)) / 2, (ymin + ymax - 20) / 2, text);
    }

    void trackbar::update()
    {
        if (!enabled)
        {
            mouseon = false, focused = false, pressed = false;
            return;
        }

        //更新TrackBar（到底该怎么翻译呢？）状态
        if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax && parent->mb == 1)
        {
            parent->focusid = id;
        }

        if (parent->mx >= xmin + barpos && parent->mx <= xmin + barpos + barwidth && parent->my >= ymin && parent->my <= ymax)
        {
            mouseon = true;
        }
        else
        {
            mouseon = false;
        }

        if (parent->mb == 1 && mouseon && focused)
        {
            pressed = true;
        }
        else if (parent->mbl == 0)
        {
            pressed = false;
        }

        if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        {
            parent->focusid = id;
        }

        focused = parent->focusid == id;

        if (focused && pressed)
        {
            barpos += parent->mx - parent->mxl;
        }

        if (focused)
        {
            if (parent->upkp && !parent->upkpl)
            {
                barpos -= 1;
            }

            if (parent->downkp && !parent->downkpl)
            {
                barpos += 1;
            }

            if (parent->leftkp)
            {
                barpos -= 1;
            }

            if (parent->rightkp)
            {
                barpos += 1;
            }
        }

        if (barpos <= 0)
        {
            barpos = 0;
        }

        if (barpos >= xmax - xmin - barwidth)
        {
            barpos = xmax - xmin - barwidth - 1;
        }

    }

    void trackbar::render()
    {

        //渲染TrackBar（How can I translate it?）
        float fcR, fcG, fcB, fcA;
        float bcR, bcG, bcB, bcA;
        fcR = FgR;
        fcG = FgG;
        fcB = FgB;
        fcA = FgA;
        bcR = BgR;
        bcG = BgG;
        bcB = BgB;
        bcA = BgA;

        if (mouseon)
        {
            fcR = FgR * 1.2f;
            fcG = FgG * 1.2f;
            fcB = FgB * 1.2f;
            fcA = FgA * 0.8f;
        }

        if (pressed)
        {
            fcR = FgR * 0.8f;
            fcG = FgG * 0.8f;
            fcB = FgB * 0.8f;
            fcA = FgA * 1.5f;
        }

        if (!enabled)
        {
            fcR = FgR * 0.5f;
            fcG = FgG * 0.5f;
            fcB = FgB * 0.5f;
            fcA = FgA * 0.3f;
        }

        glColor4f(bcR, bcG, bcB, bcA);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glVertex2i(xmin, ymin);
        glVertex2i(xmax, ymin);
        glVertex2i(xmax, ymax);
        glVertex2i(xmin, ymax);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glColor4f(fcR, fcG, fcB, fcA);
        glBegin(GL_QUADS);
        glVertex2i(xmin + barpos, ymin);
        glVertex2i(xmin + barpos + barwidth, ymin);
        glVertex2i(xmin + barpos + barwidth, ymax);
        glVertex2i(xmin + barpos, ymax);
        glEnd();
        glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);

        if (!enabled)
        {
            glColor4f(0.5f, 0.5f, 0.5f, linealpha);
        }

        glBegin(GL_LINE_LOOP);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glBegin(GL_LINE_LOOP);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmin + 1, ymin + 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmin + 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmax - 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmax - 1, ymin + 1);

        glEnd();
        glEnable(GL_TEXTURE_2D);
        TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);

        if (!enabled)
        {
            TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
        }

        TextRenderer::renderString((xmin + xmax - TextRenderer::getStrWidth(text)) / 2, ymin, text);

    }

    void textbox::update()
    {
        if (!enabled)
        {
            mouseon = false, focused = false, pressed = false;
            return;
        }

        static int delt = 0;
        static int ldel = 0;

        if (delt > INT_MAX - 2)
        {
            delt = 0;
        }

        if (ldel > INT_MAX - 2)
        {
            delt = 0;
        }

        //更新文本框状态
        if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        {
            mouseon = true, parent->MouseOnTextbox = true;
        }
        else
        {
            mouseon = false;
        }

        if ((parent->mb == 1 && mouseon || parent->enterp) && focused)
        {
            pressed = true;
        }
        else
        {
            pressed = false;
        }

        if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        {
            parent->focusid = id;    //焦点在此
        }

        if (parent->focusid == id)
        {
            focused = true;
        }
        else
        {
            focused = false;    //焦点
        }

        if (focused && inputstr != "")
        {
            text += inputstr;
        }

        delt++;

        if (parent->backspacep && (delt - ldel > 50) && text.length() >= 1)
        {
            ldel = delt;
            int n = text[text.length() - 1];

            if (n > 0 && n <= 127)
            {
                text = text.substr(0, text.length() - 1);
            }
            else
            {
                text = text.substr(0, text.length() - 2);
            }
        }
    }

    void textbox::render()
    {

        //渲染文本框
        float bcR, bcG, bcB, bcA;
        bcR = BgR;
        bcG = BgG;
        bcB = BgB;
        bcA = BgA;

        if (!enabled)
        {
            bcR = BgR * 0.5f;
            bcG = BgG * 0.5f;
            bcB = BgB * 0.5f;
            bcA = BgA * 0.3f;
        }

        glColor4f(bcR, bcG, bcB, bcA);

        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();
        glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);

        if (!enabled)
        {
            glColor4f(0.5f, 0.5f, 0.5f, linealpha);
        }

        glLineWidth(linewidth);
        glBegin(GL_LINE_LOOP);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();

        glBegin(GL_LINE_LOOP);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmin + 1, ymin + 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmin + 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmax - 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmax - 1, ymin + 1);

        glEnd();

        glEnable(GL_TEXTURE_2D);
        TextRenderer::setFontColor(1.0f, 1.0f, 1.0f, 1.0f);

        if (!enabled)
        {
            TextRenderer::setFontColor(0.6f, 0.6f, 0.6f, 1.0f);
        }

        TextRenderer::renderString(xmin, (ymin + ymax - 20) / 2, text);

    }

    void vscroll::update()
    {
        if (!enabled)
        {
            mouseon = false, focused = false, pressed = false;
            return;
        }

        static double lstime;
        msup = false;
        msdown = false;
        psup = false;
        psdown = false;

        //更新滚动条状态
        //鼠标悬停
        mouseon = (parent->my >= ymin + barpos + 20 && parent->my <= ymin + barpos + barheight + 20 && parent->mx >= xmin && parent->mx <= xmax);

        if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        {
            if (parent->mb == 1)
            {
                parent->focusid = id;
            }

            if (parent->my <= ymin + 20)
            {
                msup = true;

                if (parent->mb == 1 && parent->mbl == 0)
                {
                    barpos -= 10;
                }

                if (parent->mb == 1)
                {
                    psup = true;
                }
            }
            else if (parent->my >= ymax - 20)
            {
                msdown = true;

                if (parent->mb == 1 && parent->mbl == 0)
                {
                    barpos += 10;
                }

                if (parent->mb == 1)
                {
                    psdown = true;
                }
            }
            else if (timer() - lstime > 0.1 && parent->mb == 1)
            {
                lstime = timer();

                if (parent->my < ymin + barpos + 20)
                {
                    barpos -= 25;
                }

                if (parent->my > ymin + barpos + barheight + 20)
                {
                    barpos += 25;
                }
            }
        }

        if (parent->mb == 1 && mouseon && focused)  //鼠标按住
        {
            pressed = true;
        }
        else
        {
            if (parent->mbl == 0)
            {
                pressed = false;
            }
        }

        if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        {
            parent->focusid = id;    //焦点在此
        }

        focused = (parent->focusid == id);   //焦点

        if (pressed)
        {
            barpos += parent->my - parent->myl;    //拖动
        }

        if (focused)
        {
            if (parent->upkp)
            {
                barpos -= 1;
            }

            if (parent->downkp)
            {
                barpos += 1;
            }

            if (parent->leftkp && !parent->leftkpl)
            {
                barpos -= 1;
            }

            if (parent->rightkp && !parent->rightkpl)
            {
                barpos += 1;
            }
        }

        if (defaultv)
        {
            barpos += (parent->mwl - parent->mw) * 15;
        }

        if (barpos < 0)
        {
            barpos = 0;    //让拖动条不越界
        }

        if (barpos >= ymax - ymin - barheight - 40)
        {
            barpos = ymax - ymin - barheight - 40;
        }
    }

    void vscroll::render()
    {
        //渲染滚动条
        float fcR, fcG, fcB, fcA;
        float bcR, bcG, bcB, bcA;
        fcR = FgR;
        fcG = FgG;
        fcB = FgB;
        fcA = FgA;
        bcR = BgR;
        bcG = BgG;
        bcB = BgB;
        bcA = BgA;

        if (mouseon)
        {
            fcR = FgR * 1.2f;
            fcG = FgG * 1.2f;
            fcB = FgB * 1.2f;
            fcA = FgA * 0.8f;
        }

        if (pressed)
        {
            fcR = FgR * 0.8f;
            fcG = FgG * 0.8f;
            fcB = FgB * 0.8f;
            fcA = FgA * 1.5f;
        }

        if (!enabled)
        {
            fcR = FgR * 0.5f;
            fcG = FgG * 0.5f;
            fcB = FgB * 0.5f;
            fcA = FgA * 0.3f;
        }

        glColor4f(bcR, bcG, bcB, bcA);                                              //Track
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glVertex2i(xmin, ymin);
        glVertex2i(xmax, ymin);
        glVertex2i(xmax, ymax);
        glVertex2i(xmin, ymax);
        glEnd();
        glDisable(GL_TEXTURE_2D);                                                //Bar
        glColor4f(fcR, fcG, fcB, fcA);
        glBegin(GL_QUADS);
        glVertex2i(xmin, ymin + barpos + 20);
        glVertex2i(xmin, ymin + barpos + barheight + 20);
        glVertex2i(xmax, ymin + barpos + barheight + 20);
        glVertex2i(xmax, ymin + barpos + 20);
        glEnd();

        if (msup)
        {
            glColor4f(1.0f, 1.0f, 1.0f, 0.7f);

            if (psup)
            {
                glColor4f(FgR, FgG, FgB, 0.9f);
            }

            glBegin(GL_QUADS);
            glVertex2i(xmin, ymin);
            glVertex2i(xmin, ymin + 20);
            glVertex2i(xmax, ymin + 20);
            glVertex2i(xmax, ymin);
            glEnd();
        }

        if (msdown)
        {
            glColor4f(1.0f, 1.0f, 1.0f, 0.7f);

            if (psdown)
            {
                glColor4f(FgR, FgG, FgB, 0.9f);
            }

            glBegin(GL_QUADS);
            glVertex2i(xmin, ymax - 20);
            glVertex2i(xmin, ymax);
            glVertex2i(xmax, ymax);
            glVertex2i(xmax, ymax - 20);
            glEnd();
        }

        glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);

        if (!enabled)
        {
            glColor4f(0.5f, 0.5f, 0.5f, linealpha);
        }

        glBegin(GL_LINE_LOOP);
        glVertex2i(xmin, ymin);
        glVertex2i(xmin, ymax);
        glVertex2i(xmax, ymax);
        glVertex2i(xmax, ymin);
        glEnd();

        glBegin(GL_LINE_LOOP);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmin + 1, ymin + 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmin + 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.4f, 0.4f, 0.4f, linealpha);
        }

        glVertex2i(xmax - 1, ymax - 1);

        if (focused)
        {
            glColor4f(1.0f, 1.0f, 1.0f, linealpha);
        }
        else
        {
            glColor4f(0.8f, 0.8f, 0.8f, linealpha);
        }

        glVertex2i(xmax - 1, ymin + 1);

        glEnd();

        glLineWidth(3.0);
        glBegin(GL_LINES);
        glColor4f(FgR, FgG, FgB, 1.0);

        if (psup)
        {
            glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
        }

        glVertex2i((xmin + xmax) / 2, ymin + 8);
        glVertex2i((xmin + xmax) / 2 - 4, ymin + 12);
        glVertex2i((xmin + xmax) / 2, ymin + 8);
        glVertex2i((xmin + xmax) / 2 + 4, ymin + 12);
        glColor4f(FgR, FgG, FgB, 1.0);

        if (psdown)
        {
            glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
        }

        glVertex2i((xmin + xmax) / 2, ymax - 8);
        glVertex2i((xmin + xmax) / 2 - 4, ymax - 12);
        glVertex2i((xmin + xmax) / 2, ymax - 8);
        glVertex2i((xmin + xmax) / 2 + 4, ymax - 12);
        glEnd();
    }

    void imagebox::update()
    {

    }

    void imagebox::render()
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, imageid);
        glBegin(GL_QUADS);
        glTexCoord2f(txmin, tymax);
        glVertex2i(xmin, ymin);
        glTexCoord2f(txmin, tymin);
        glVertex2i(xmin, ymax);
        glTexCoord2f(txmax, tymin);
        glVertex2i(xmax, ymax);
        glTexCoord2f(txmax, tymax);
        glVertex2i(xmax, ymin);
        glEnd();
    }

    void Form::Init()
    {
        maxid = 0;
        currentid = 0;
        focusid = -1;
        childrenCount = 0;

        //Transition forward
        if (transitionList != 0)
        {
            glDeleteLists(transitionList, 1);
        }

        transitionList = lastdisplaylist;
        transitionForward = true;
        transitionTimer = timer();
    }

    void Form::registerControl(controls &c)
    {
        c.id = currentid;
        c.parent = this;
        children.push_back(&c);
        currentid++;
        maxid++;
        childrenCount++;
    }

    void Form::update()
    {

        int i;
        updated = false;
        bool lMouseOnTextbox = MouseOnTextbox;
        MouseOnTextbox = false;

        if (glfwGetKey(MainWindow, GLFW_KEY_TAB) == GLFW_PRESS)                              //TAB键切换焦点
        {
            if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)    //Shift+Tab
            {
                updated = true;

                if (!tabp)
                {
                    focusid--;
                }

                if (focusid == -2)
                {
                    focusid = maxid - 1;    //到了最前一个ID
                }
            }
            else
            {
                updated = true;

                if (!tabp)
                {
                    focusid++;
                }

                if (focusid == maxid + 1)
                {
                    focusid = -1;    //到了最后一个ID
                }
            }

            tabp = true;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_TAB) != GLFW_PRESS)
        {
            tabp = false;
        }

        if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS))
        {
            shiftp = false;
        }

        enterpl = enterp;

        if (glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS)
        {
            updated = true;
            enterp = true;
        }

        if (!(glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS))
        {
            enterp = false;
        }

        upkpl = upkp;                                                              //方向键上

        if (glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS)
        {
            updated = true;
            upkp = true;
        }

        if (!(glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS))
        {
            upkp = false;
        }

        downkpl = downkp;                                                          //方向键下

        if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            downkp = true;
        }

        if (!(glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS))
        {
            downkp = false;
        }

        leftkpl = leftkp;                                                          //方向键左

        if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            leftkp = true;
        }

        if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS))
        {
            leftkp = false;
        }

        rightkpl = rightkp;                                                        //方向键右

        if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            rightkp = true;
        }

        if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) != GLFW_PRESS)
        {
            rightkp = false;
        }

        backspacepl = backspacep;

        if (glfwGetKey(MainWindow, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
        {
            backspacep = true;
        }
        else
        {
            backspacep = false;
        }

        if (mb == 1 && mbl == 0)
        {
            focusid = -1;    //空点击时使焦点清空
        }

        for (i = 0; i != childrenCount; i++)
        {
            children[i]->updatepos();
            children[i]->update();                                               //更新子控件
        }

        if (!lMouseOnTextbox && MouseOnTextbox)
        {
            glfwDestroyCursor(MouseCursor);
            MouseCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
            glfwSetCursor(MainWindow, MouseCursor);
        }

        if (lMouseOnTextbox && !MouseOnTextbox)
        {
            glfwDestroyCursor(MouseCursor);
            MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            glfwSetCursor(MainWindow, MouseCursor);
        }

        onUpdate();

    }

    void Form::render()
    {
        Background();

        double TimeDelta = timer() - transitionTimer;
        float transitionAnim = (float)(1.0 - pow(0.8, TimeDelta * 60.0) + pow(0.8, 0.3 * 60.0) / 0.3 * TimeDelta);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, windowwidth, windowheight, 0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glDepthFunc(GL_ALWAYS);
        glLoadIdentity();

        if (GUIScreenBlur)
        {
            screenBlur();
        }

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LINE_SMOOTH);

        if (TimeDelta <= 0.3 && transitionList != 0)
        {
            if (transitionForward)
            {
                glTranslatef(-transitionAnim * windowwidth, 0.0f, 0.0f);
            }
            else
            {
                glTranslatef(transitionAnim * windowwidth, 0.0f, 0.0f);
            }

            glCallList(transitionList);
            glLoadIdentity();

            if (transitionForward)
            {
                glTranslatef(windowwidth - transitionAnim * windowwidth, 0.0f, 0.0f);
            }
            else
            {
                glTranslatef(transitionAnim * windowwidth - windowwidth, 0.0f, 0.0f);
            }
        }
        else if (transitionList != 0)
        {
            glDeleteLists(transitionList, 1);
            transitionList = 0;
        }

        if (displaylist == 0)
        {
            displaylist = glGenLists(1);
        }

        glNewList(displaylist, GL_COMPILE_AND_EXECUTE);

        for (int i = 0; i != childrenCount; i++)
        {
            children[i]->render();
        }

        onRender();
        glEndList();
        lastdisplaylist = displaylist;

    }

    label::label(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        text = t;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    button::button(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        text = t;
        enabled = true;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    trackbar::trackbar(string t, int w, int s, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        text = t;
        enabled = true;
        barwidth = w;
        barpos = s;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    textbox::textbox(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        text = t;
        enabled = true;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    vscroll::vscroll(int h, int s, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        enabled = true;
        barheight = h;
        barpos = s;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    imagebox::imagebox(float _txmin, float _txmax, float _tymin, float _tymax, TextureID iid, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b)
    {
        txmin = _txmin;
        txmax = _txmax;
        tymin = _tymin;
        tymax = _tymax;
        imageid = iid;
        resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
    }

    void Form::cleanup()
    {
        //Transition backward
        if (transitionList != 0)
        {
            glDeleteLists(transitionList, 1);
        }

        transitionList = displaylist;
        transitionForward = false;
        transitionTimer = timer();

        for (int i = 0; i != childrenCount; i++)
        {
            children[i]->destroy();
        }

        childrenCount = 0;
    }

    controls *Form::getControlByID(int cid)
    {
        for (int i = 0; i != childrenCount; i++)
        {
            if (children[i]->id == cid)
            {
                return children[i];
            }
        }

        return nullptr;
    }

    Form::Form()
    {
        Init();
    }
    void Form::start()
    {
        double dmx, dmy;
        glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glDisable(GL_CULL_FACE);
        TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
        onLoad();
        ExitSignal = false;

        do
        {
            mxl = mx;
            myl = my;
            mwl = mw;
            mbl = mb;
            mb = getMouseButton();
            mw = getMouseScroll();
            glfwGetCursorPos(MainWindow, &dmx, &dmy);
            mx = (int)dmx, my = (int)dmy;
            update();
            render();
            glfwSwapBuffers(MainWindow);
            glfwPollEvents();

            if (ExitSignal)
            {
                onLeaving();
            }

            if (glfwWindowShouldClose(MainWindow))
            {
                exit(0);
            }
        }
        while (!ExitSignal);

        onLeave();
    }
    Form::~Form()
    {
        cleanup();
    }

}
