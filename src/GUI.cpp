#include "GUI.h"
#include "Framebuffer.h"
#include "FrustumTest.h"
#include "Renderer.h"
#include "TextRenderer.h"

namespace GUI {

float linealpha = 0.9f;
float FgR = 0.2f;
float FgG = 0.2f;
float FgB = 0.2f;
float FgA = 0.6f;
float BgR = 0.2f;
float BgG = 0.2f;
float BgB = 0.2f;
float BgA = 0.3f;
unsigned int transitionList = 0;
unsigned int lastdisplaylist = 0;
double transitionTimer;
bool transitionForward;

void clearTransition() {
    if (transitionList != 0) {
        glDeleteLists(transitionList, 1);
        transitionList = 0;
    }
    if (lastdisplaylist != 0) {
        glDeleteLists(lastdisplaylist, 1);
        lastdisplaylist = 0;
    }
}

void drawBackground() {
    static double startTimer = Timer();
    double elapsed = Timer() - startTimer;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixf(Mat4f::perspective(static_cast<float>(Pi / 2.0), (float) WindowWidth / WindowHeight, 0.1f, 10.0f)
                      .transpose()
                      .data);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotated(elapsed * 4.0, 0.1, 1.0, 0.1);

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[0]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, -1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[1]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[2]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[3]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, -1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[4]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, UIBackgroundTextures[5]);
    glBegin(GL_QUADS);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(-1.0f, 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, -1.0f);
    glEnd();
}

void UIVertex(double x, double y) {
    glVertex2d(x * Stretch, y * Stretch);
}

void UIVertex(int x, int y) {
    glVertex2i(static_cast<int>(x * Stretch), static_cast<int>(y * Stretch));
}

void UISetFontColor(float r, float g, float b, float a) {
    TextRenderer::setFontColor(r, g, b, a);
}

void UIRenderString(int xmin, int xmax, int ymin, int ymax, std::string const& s, bool centered) {
    xmin = static_cast<int>(xmin * Stretch);
    xmax = static_cast<int>(xmax * Stretch);
    ymin = static_cast<int>(ymin * Stretch);
    ymax = static_cast<int>(ymax * Stretch);
    if (centered)
        TextRenderer::renderString(
            (xmin + xmax - TextRenderer::getStringWidth(s)) / 2,
            (ymin + ymax - TextRenderer::getLineHeight()) / 2,
            s
        );
    else
        TextRenderer::renderString(xmin, (ymin + ymax - TextRenderer::getLineHeight()) / 2, s);
}

void UIRenderString(int xmin, int xmax, int ymin, int ymax, std::u32string const& s, bool centered) {
    xmin = static_cast<int>(xmin * Stretch);
    xmax = static_cast<int>(xmax * Stretch);
    ymin = static_cast<int>(ymin * Stretch);
    ymax = static_cast<int>(ymax * Stretch);
    if (centered)
        TextRenderer::renderString(
            (xmin + xmax - TextRenderer::getStringWidth(s)) / 2,
            (ymin + ymax - TextRenderer::getLineHeight()) / 2,
            s
        );
    else
        TextRenderer::renderString(xmin, (ymin + ymax - TextRenderer::getLineHeight()) / 2, s);
}

void Control::updatepos() {
    xmin = (int) (WindowWidth * _xmin_b / Stretch) + _xmin_r;
    ymin = (int) (WindowHeight * _ymin_b / Stretch) + _ymin_r;
    xmax = (int) (WindowWidth * _xmax_b / Stretch) + _xmax_r;
    ymax = (int) (WindowHeight * _ymax_b / Stretch) + _ymax_r;
}

void Control::resize(int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) {
    _xmin_r = xi_r;
    _xmax_r = xa_r;
    _ymin_r = yi_r;
    _ymax_r = ya_r;
    _xmin_b = xi_b;
    _xmax_b = xa_b;
    _ymin_b = yi_b;
    _ymax_b = ya_b;
}

void Label::update() {
    if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        mouseon = true;
    else
        mouseon = false;

    if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        parent->focusid = id;
    focused = parent->focusid == id;
}

void Label::render() {

    float fcR, fcG, fcB, fcA;
    fcR = FgR;
    fcG = FgG;
    fcB = FgB;
    fcA = FgA;
    if (mouseon) {
        fcR = FgR * 1.2f;
        fcG = FgG * 1.2f;
        fcB = FgB * 1.2f;
        fcA = FgA * 0.8f;
    }
    if (focused) {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINE_LOOP);
        glColor4f(FgR * 0.6f, FgG * 0.6f, FgB * 0.6f, linealpha);
        UIVertex(xmin, ymin);
        UIVertex(xmin, ymax);
        UIVertex(xmax, ymax);
        UIVertex(xmax, ymin);
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
    UISetFontColor(fcR, fcG, fcB, fcA);
    UIRenderString(xmin, xmax, ymin, ymax, text, centered);
}

void Button::update() {
    if (!enabled) {
        mouseon = false, focused = false, pressed = false, clicked = false;
        return;
    }

    if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        mouseon = true;
    else
        mouseon = false;

    if ((parent->mb == 1 && mouseon || parent->enterp) && focused)
        pressed = true;
    else
        pressed = false;

    if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        parent->focusid = id;
    if (parent->focusid == id)
        focused = true;
    else
        focused = false;

    clicked = (parent->mb == 0 && parent->mbl == 1 && mouseon || parent->enterpl && parent->enterp == false) && focused;
}

void Button::render() {

    float fcR, fcG, fcB, fcA;
    fcR = FgR;
    fcG = FgG;
    fcB = FgB;
    fcA = FgA;
    if (mouseon) {
        fcR = FgR * 1.2f;
        fcG = FgG * 1.2f;
        fcB = FgB * 1.2f;
        fcA = FgA * 0.8f;
    }
    if (pressed) {
        fcR = FgR * 0.8f;
        fcG = FgG * 0.8f;
        fcB = FgB * 0.8f;
        fcA = FgA * 1.5f;
    }
    if (!enabled) {
        fcR = FgR * 0.5f;
        fcG = FgG * 0.5f;
        fcB = FgB * 0.5f;
        fcA = FgA * 0.3f;
    }

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glColor4f(fcR, fcG, fcB, fcA);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);
    if (focused)
        glColor4f(1.0f, 1.0f, 1.0f, linealpha);
    if (!enabled)
        glColor4f(0.5f, 0.5f, 0.5f, linealpha);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    UISetFontColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (!enabled)
        UISetFontColor(0.6f, 0.6f, 0.6f, 1.0f);
    UIRenderString(xmin, xmax, ymin, ymax, text, true);
}

void Trackbar::update() {
    if (!enabled) {
        mouseon = false, focused = false, pressed = false;
        return;
    }

    if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax && parent->mb == 1)
        parent->focusid = id;
    if (parent->mx >= xmin + barpos && parent->mx <= xmin + barpos + barwidth && parent->my >= ymin
        && parent->my <= ymax)
        mouseon = true;
    else
        mouseon = false;
    if (parent->mb == 1 && mouseon && focused)
        pressed = true;
    else if (parent->mbl == 0)
        pressed = false;
    if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        parent->focusid = id;
    focused = parent->focusid == id;
    if (focused && pressed)
        barpos += parent->mx - parent->mxl;
    if (focused) {
        if (parent->upkp && !parent->upkpl)
            barpos -= 1;
        if (parent->downkp && !parent->downkpl)
            barpos += 1;
        if (parent->leftkp)
            barpos -= 1;
        if (parent->rightkp)
            barpos += 1;
    }
    if (barpos <= 0)
        barpos = 0;
    if (barpos >= xmax - xmin - barwidth)
        barpos = xmax - xmin - barwidth - 1;
}

void Trackbar::render() {

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
    if (mouseon) {
        fcR = FgR * 1.2f;
        fcG = FgG * 1.2f;
        fcB = FgB * 1.2f;
        fcA = FgA * 0.8f;
    }
    if (pressed) {
        fcR = FgR * 0.8f;
        fcG = FgG * 0.8f;
        fcB = FgB * 0.8f;
        fcA = FgA * 1.5f;
    }
    if (!enabled) {
        fcR = FgR * 0.5f;
        fcG = FgG * 0.5f;
        fcB = FgB * 0.5f;
        fcA = FgA * 0.3f;
    }

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glColor4f(bcR, bcG, bcB, bcA);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(fcR, fcG, fcB, fcA);
    UIVertex(xmin + barpos, ymin);
    UIVertex(xmin + barpos, ymax);
    UIVertex(xmin + barpos + barwidth, ymax);
    UIVertex(xmin + barpos + barwidth, ymin);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);
    if (focused)
        glColor4f(1.0f, 1.0f, 1.0f, linealpha);
    if (!enabled)
        glColor4f(0.5f, 0.5f, 0.5f, linealpha);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    UISetFontColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (!enabled)
        UISetFontColor(0.6f, 0.6f, 0.6f, 1.0f);
    UIRenderString(xmin, xmax, ymin, ymax, text, true);
}

void TextBox::update() {
    if (!enabled) {
        mouseon = false, focused = false, pressed = false;
        return;
    }

    if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax)
        mouseon = true, parent->MouseOnTextbox = true;
    else
        mouseon = false;

    if ((parent->mb == 1 && mouseon || parent->enterp) && focused)
        pressed = true;
    else
        pressed = false;

    if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        parent->focusid = id;
    if (parent->focusid == id)
        focused = true;
    else
        focused = false;

    if (focused) {
        activated = true;
        if (!inputstr.empty())
            input += inputstr;
        if (backspace && !input.empty())
            input = input.substr(0, input.length() - 1);
    }
}

void TextBox::render() {
    float bcR, bcG, bcB, bcA;
    bcR = BgR;
    bcG = BgG;
    bcB = BgB;
    bcA = BgA;
    if (!enabled) {
        bcR = BgR * 0.5f;
        bcG = BgG * 0.5f;
        bcB = BgB * 0.5f;
        bcA = BgA * 0.3f;
    }

    glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glColor4f(bcR, bcG, bcB, bcA);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);
    if (focused)
        glColor4f(1.0f, 1.0f, 1.0f, linealpha);
    if (!enabled)
        glColor4f(0.5f, 0.5f, 0.5f, linealpha);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    UISetFontColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (!enabled)
        UISetFontColor(0.6f, 0.6f, 0.6f, 1.0f);
    if (activated)
        UIRenderString(xmin, xmax, ymin, ymax, input, false);
    else
        UIRenderString(xmin, xmax, ymin, ymax, text, false);
}

void VScroll::update() {
    if (!enabled) {
        mouseon = false, focused = false, pressed = false;
        return;
    }
    static double lstime;
    msup = false;
    msdown = false;
    psup = false;
    psdown = false;

    mouseon =
        (parent->my >= ymin + barpos + 20 && parent->my <= ymin + barpos + barheight + 20 && parent->mx >= xmin
         && parent->mx <= xmax);
    if (parent->mx >= xmin && parent->mx <= xmax && parent->my >= ymin && parent->my <= ymax) {
        if (parent->mb == 1)
            parent->focusid = id;
        if (parent->my <= ymin + 20) {
            msup = true;
            if (parent->mb == 1 && parent->mbl == 0)
                barpos -= 10;
            if (parent->mb == 1)
                psup = true;
        } else if (parent->my >= ymax - 20) {
            msdown = true;
            if (parent->mb == 1 && parent->mbl == 0)
                barpos += 10;
            if (parent->mb == 1)
                psdown = true;
        } else if (Timer() - lstime > 0.1 && parent->mb == 1) {
            lstime = Timer();
            if (parent->my < ymin + barpos + 20)
                barpos -= 25;
            if (parent->my > ymin + barpos + barheight + 20)
                barpos += 25;
        }
    }
    if (parent->mb == 1 && mouseon && focused)
        pressed = true;
    else {
        if (parent->mbl == 0)
            pressed = false;
    }

    if (parent->mb == 1 && parent->mbl == 0 && mouseon)
        parent->focusid = id;
    focused = (parent->focusid == id);
    if (pressed)
        barpos += parent->my - parent->myl;
    if (focused) {
        if (parent->upkp)
            barpos -= 1;
        if (parent->downkp)
            barpos += 1;
        if (parent->leftkp && !parent->leftkpl)
            barpos -= 1;
        if (parent->rightkp && !parent->rightkpl)
            barpos += 1;
    }
    if (defaultv)
        barpos += (parent->mwl - parent->mw) * 15;
    if (barpos < 0)
        barpos = 0;
    if (barpos >= ymax - ymin - barheight - 40)
        barpos = ymax - ymin - barheight - 40;
}

void VScroll::render() {

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
    if (mouseon) {
        fcR = FgR * 1.2f;
        fcG = FgG * 1.2f;
        fcB = FgB * 1.2f;
        fcA = FgA * 0.8f;
    }
    if (pressed) {
        fcR = FgR * 0.8f;
        fcG = FgG * 0.8f;
        fcB = FgB * 0.8f;
        fcA = FgA * 1.5f;
    }
    if (!enabled) {
        fcR = FgR * 0.5f;
        fcG = FgG * 0.5f;
        fcB = FgB * 0.5f;
        fcA = FgA * 0.3f;
    }

    glDisable(GL_TEXTURE_2D);

    glColor4f(bcR, bcG, bcB, bcA);
    glBegin(GL_QUADS);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(fcR, fcG, fcB, fcA);
    UIVertex(xmin, ymin + barpos + 20);
    UIVertex(xmin, ymin + barpos + barheight + 20);
    UIVertex(xmax, ymin + barpos + barheight + 20);
    UIVertex(xmax, ymin + barpos + 20);
    glEnd();

    if (msup) {
        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
        if (psup)
            glColor4f(FgR, FgG, FgB, 0.9f);
        UIVertex(xmin, ymin);
        UIVertex(xmin, ymin + 20);
        UIVertex(xmax, ymin + 20);
        UIVertex(xmax, ymin);
        glEnd();
    }
    if (msdown) {
        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
        if (psdown)
            glColor4f(FgR, FgG, FgB, 0.9f);
        UIVertex(xmin, ymax - 20);
        UIVertex(xmin, ymax);
        UIVertex(xmax, ymax);
        UIVertex(xmax, ymax - 20);
        glEnd();
    }

    glBegin(GL_LINE_LOOP);
    glColor4f(FgR * 0.9f, FgG * 0.9f, FgB * 0.9f, linealpha);
    if (focused)
        glColor4f(1.0f, 1.0f, 1.0f, linealpha);
    if (!enabled)
        glColor4f(0.5f, 0.5f, 0.5f, linealpha);
    UIVertex(xmin, ymin);
    UIVertex(xmin, ymax);
    UIVertex(xmax, ymax);
    UIVertex(xmax, ymin);
    glEnd();

    glBegin(GL_LINES);
    glColor4f(FgR, FgG, FgB, 1.0);
    if (psup)
        glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
    UIVertex((xmin + xmax) / 2, ymin + 8);
    UIVertex((xmin + xmax) / 2 - 4, ymin + 12);
    UIVertex((xmin + xmax) / 2, ymin + 8);
    UIVertex((xmin + xmax) / 2 + 4, ymin + 12);
    glColor4f(FgR, FgG, FgB, 1.0);
    if (psdown)
        glColor4f(1.0f - FgR, 1.0f - FgG, 1.0f - FgB, 1.0f);
    UIVertex((xmin + xmax) / 2, ymax - 8);
    UIVertex((xmin + xmax) / 2 - 4, ymax - 12);
    UIVertex((xmin + xmax) / 2, ymax - 8);
    UIVertex((xmin + xmax) / 2 + 4, ymax - 12);
    glEnd();

    glEnable(GL_TEXTURE_2D);
}

void ImageBox::update() {}

void ImageBox::render() {
    glBindTexture(GL_TEXTURE_2D, imageid);
    glBegin(GL_QUADS);
    glTexCoord2f(txmin, tymax);
    UIVertex(xmin, ymin);
    glTexCoord2f(txmin, tymin);
    UIVertex(xmin, ymax);
    glTexCoord2f(txmax, tymin);
    UIVertex(xmax, ymax);
    glTexCoord2f(txmax, tymax);
    UIVertex(xmax, ymin);
    glEnd();
}

void Form::registerControl(Control* c) {
    c->id = currentid;
    c->parent = this;
    children.push_back(c);
    currentid++;
    maxid++;
}

void Form::registerControls(std::initializer_list<Control*> cs) {
    for (auto const c: cs)
        registerControl(c);
}

void Form::update() {
    updated = false;
    bool lMouseOnTextbox = MouseOnTextbox;
    MouseOnTextbox = false;

    if (glfwGetKey(MainWindow, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
            || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            updated = true;
            if (!tabp)
                focusid--;
            if (focusid == -2)
                focusid = maxid - 1;
        } else {
            updated = true;
            if (!tabp)
                focusid++;
            if (focusid == maxid + 1)
                focusid = -1;
        }
        tabp = true;
    }
    if (glfwGetKey(MainWindow, GLFW_KEY_TAB) != GLFW_PRESS)
        tabp = false;
    if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
          || glfwGetKey(MainWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS))
        shiftp = false;

    enterpl = enterp;
    if (glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS) {
        updated = true;
        enterp = true;
    }
    if (!(glfwGetKey(MainWindow, GLFW_KEY_ENTER) == GLFW_PRESS))
        enterp = false;

    upkpl = upkp;
    if (glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS) {
        updated = true;
        upkp = true;
    }
    if (!(glfwGetKey(MainWindow, GLFW_KEY_UP) == GLFW_PRESS))
        upkp = false;

    downkpl = downkp;
    if (glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS)
        downkp = true;
    if (!(glfwGetKey(MainWindow, GLFW_KEY_DOWN) == GLFW_PRESS))
        downkp = false;

    leftkpl = leftkp;
    if (glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS)
        leftkp = true;
    if (!(glfwGetKey(MainWindow, GLFW_KEY_LEFT) == GLFW_PRESS))
        leftkp = false;
    rightkpl = rightkp;
    if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) == GLFW_PRESS)
        rightkp = true;
    if (glfwGetKey(MainWindow, GLFW_KEY_RIGHT) != GLFW_PRESS)
        rightkp = false;

    if (mb == 1 && mbl == 0)
        focusid = -1;

    for (size_t i = 0; i != children.size(); i++) {
        children[i]->updatepos();
        children[i]->update();
    }

    if (!lMouseOnTextbox && MouseOnTextbox) {
        glfwDestroyCursor(MouseCursor);
        MouseCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        glfwSetCursor(MainWindow, MouseCursor);
    }
    if (lMouseOnTextbox && !MouseOnTextbox) {
        glfwDestroyCursor(MouseCursor);
        MouseCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        glfwSetCursor(MainWindow, MouseCursor);
    }
    onUpdate();
}

void Form::render() {
    if (UIBackgroundBlur) {
        static Framebuffer fbo;
        float step = 2.0f;
        float upscaling = 2.0f;
        float sigma = 16.0f / upscaling;

        int width = int(WindowWidth / upscaling);
        int height = int(WindowHeight / upscaling);
        if (fbo.width() != width || fbo.height() != height) {
            fbo = Framebuffer(width, height, 2, false, false, true);
        }

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        fbo.bindTarget({0});
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawBackground();

        auto& shader = Renderer::shaders[Renderer::FilterShader];
        shader.bind();
        shader.setUniformI("u_buffer", 0);
        shader.setUniform("u_buffer_width", static_cast<float>(fbo.width()));
        shader.setUniform("u_buffer_height", static_cast<float>(fbo.height()));
        shader.setUniform("u_gaussian_blur_radius", 2.0f * sigma);
        shader.setUniform("u_gaussian_blur_step_size", step);
        shader.setUniform("u_gaussian_blur_sigma", sigma);

        fbo.bindColorTexture(0);
        fbo.bindTarget({1});
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.setUniformI("u_filter_id", 1);
        Renderer::Begin(GL_QUADS, 2, 2, 0);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex2i(0, 0);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex2i(0, fbo.height());
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex2i(fbo.width(), fbo.height());
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex2i(fbo.width(), 0);
        Renderer::End().render();

        fbo.bindColorTexture(1);
        fbo.bindTarget({0});
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.setUniformI("u_filter_id", 2);
        Renderer::Begin(GL_QUADS, 2, 2, 0);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex2i(0, 0);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex2i(0, fbo.height());
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex2i(fbo.width(), fbo.height());
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex2i(fbo.width(), 0);
        Renderer::End().render();

        fbo.bindColorTexture(0);
        fbo.unbindTarget();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.unbind();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, WindowWidth, WindowHeight, 0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2i(0, 0);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, WindowHeight);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2i(WindowWidth, WindowHeight);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2i(WindowWidth, 0);
        glEnd();
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawBackground();
    }

    double TimeDelta = Timer() - transitionTimer;
    float transitionAnim = (float) (1.0 - pow(0.8, TimeDelta * 60.0) + pow(0.8, 0.3 * 60.0) / 0.3 * TimeDelta);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WindowWidth, WindowHeight, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (TimeDelta <= 0.3 && transitionList != 0) {
        if (transitionForward)
            glTranslatef(-transitionAnim * WindowWidth, 0.0f, 0.0f);
        else
            glTranslatef(transitionAnim * WindowWidth, 0.0f, 0.0f);
        glCallList(transitionList);
        glLoadIdentity();
        if (transitionForward)
            glTranslatef(WindowWidth - transitionAnim * WindowWidth, 0.0f, 0.0f);
        else
            glTranslatef(transitionAnim * WindowWidth - WindowWidth, 0.0f, 0.0f);
    } else if (transitionList != 0) {
        glDeleteLists(transitionList, 1);
        transitionList = 0;
    }

    if (displaylist == 0)
        displaylist = glGenLists(1);
    glNewList(displaylist, GL_COMPILE_AND_EXECUTE);
    for (size_t i = 0; i != children.size(); i++)
        children[i]->render();
    onRender();
    glEndList();
    lastdisplaylist = displaylist;
}

Control* Form::getControlByID(int cid) {
    for (size_t i = 0; i != children.size(); i++) {
        if (children[i]->id == cid)
            return children[i];
    }
    return nullptr;
}

Form::Form() {
    transitionList = lastdisplaylist;
    transitionForward = true;
    transitionTimer = Timer();
    inputstr.clear();
    backspace = false;
}

Form::~Form() {
    if (transitionList != 0)
        glDeleteLists(transitionList, 1);
    transitionList = displaylist;
    transitionForward = false;
    transitionTimer = Timer();
}

void Form::singleloop() {
    glfwSwapBuffers(MainWindow);
    render();
    glfwPollEvents();

    mxl = mx;
    myl = my;
    mwl = mw;
    mbl = mb;
    mb = getMouseButton();
    mw = getMouseScroll();
    double dmx, dmy;
    glfwGetCursorPos(MainWindow, &dmx, &dmy);
    dmx /= Stretch;
    dmy /= Stretch;
    mx = (int) dmx, my = (int) dmy;
    if (exit)
        onLeaving();
    if (glfwWindowShouldClose(MainWindow)) {
        onLeave();
        std::exit(0);
    }
    update();
    inputstr.clear();
    backspace = false;
}

void Form::start() {
    GLFWcursor* Cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursor(MainWindow, Cursor);
    TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);
    onLoad();
    do {
        singleloop();
    } while (!exit);
    onLeave();
    glfwDestroyCursor(Cursor);
}
}
