#pragma once
#include "Definitions.h"
#include "Globalization.h"

extern int getMouseButton();
extern int getMouseScroll();
inline string BoolYesNo(bool b) {
    return b ? Globalization::GetStrbyKey("NEWorld.yes") : Globalization::GetStrbyKey("NEWorld.no");
}
inline string BoolEnabled(bool b) {
    return b ? Globalization::GetStrbyKey("NEWorld.enabled") : Globalization::GetStrbyKey("NEWorld.disabled");
}
template<typename T>
inline string strWithVar(string str, T var) {
    std::stringstream ss; ss << str << var; return ss.str();
}
template<typename T>
inline string Var2Str(T var) {
    std::stringstream ss; ss << var; return ss.str();
}

namespace GUI {
    extern float linealpha;
    extern float FgR;
    extern float FgG;
    extern float FgB;
    extern float FgA;
    extern float BgR;
    extern float BgG;
    extern float BgB;
    extern float BgA;

    extern unsigned int transitionList;
    extern unsigned int lastdisplaylist;
    extern double transitionTimer;
    extern bool transitionForward;

    void clearTransition();
    void drawBackground();

    void UIVertex(double x, double y);
    void UIVertex(int x, int y);
    void UISetFontColor(float r, float g, float b, float a);
    void UIRenderString(int xmin, int xmax, int ymin, int ymax, std::string const& s, bool centered = false);
    void UIRenderString(int xmin, int xmax, int ymin, int ymax, std::u32string const& s, bool centered = false);

    class Form;

    class Control {
    public:
        int id = -1, xmin = 0, ymin = 0, xmax = 0, ymax = 0;
        Form* parent = nullptr;

        virtual ~Control() {}
        virtual void update() {}
        virtual void render() {}

        void updatepos();
        void resize(int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b);

    private:
        int _xmin_r = 0, _ymin_r = 0, _xmax_r = 0, _ymax_r = 0;
        double _xmin_b = 0.0, _ymin_b = 0.0, _xmax_b = 0.0, _ymax_b = 0.0;
    };

    class Label : public Control {
    public:
        string text;
        bool mouseon = false;
        bool focused = false;
        bool centered = false;

        Label() {};
        Label(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            text(t) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };

    class Button : public Control {
    public:
        string text;
        bool mouseon = false;
        bool focused = false;
        bool pressed = false;
        bool clicked = false;
        bool enabled = false;

        Button() {};
        Button(string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            text(t), enabled(true) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };

    class Trackbar : public Control {
    public:
        string text;
        int barwidth = 0;
        int barpos = 0;

        bool mouseon = false;
        bool focused = false;
        bool pressed = false;
        bool enabled = false;

        Trackbar() {};
        Trackbar(string t, int w, int s, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            text(t), enabled(true), barwidth(w), barpos(s) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };

    class TextBox : public Control {
    public:
        std::string text;
        std::u32string input;
        bool mouseon = false;
        bool focused = false;
        bool pressed = false;
        bool enabled = false;
        bool activated = false;

        TextBox() {};
        TextBox(std::string t, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            text(t), enabled(true) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };

    class VScroll : public Control {
    public:
        int barheight = 0;
        int barpos = 0;
        bool mouseon = false;
        bool focused = false;
        bool pressed = false;
        bool enabled = false;
        bool defaultv = false, msup = false, msdown = false, psup = false, psdown = false;

        VScroll() {};
        VScroll(int h, int s, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            barheight(h), barpos(s), enabled(true) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };

    class ImageBox : public Control {
    public:
        TextureID imageid = 0;
        float txmin = 0.0f;
        float txmax = 1.0f;
        float tymin = 0.0f;
        float tymax = 1.0f;

        ImageBox() {};
        ImageBox(float txmin, float txmax, float tymin, float tymax, TextureID imageid, int xi_r, int xa_r, int yi_r, int ya_r, double xi_b, double xa_b, double yi_b, double ya_b) :
            txmin(txmin), txmax(txmax), tymin(tymin), tymax(tymax), imageid(imageid) {
            resize(xi_r, xa_r, yi_r, ya_r, xi_b, xa_b, yi_b, ya_b);
        }

        void update();
        void render();
    };
    
    class Form {
    public:
        std::vector<Control*> children;
        bool tabp = false, shiftp = false, enterp = false, enterpl = false;
        bool upkp = false, downkp = false, upkpl = false, downkpl = false;
        bool leftkp = false, rightkp = false, leftkpl = false, rightkpl = false;
        bool updated = false;
        int maxid = 0, currentid = 0, focusid = -1;
        int mx = 0, my = 0, mw = 0, mb = 0, mxl = 0, myl = 0, mwl = 0, mbl = 0;
        unsigned int displaylist = 0;
        bool exit = false;
        bool MouseOnTextbox = false;

        Form();
        ~Form();

        void registerControl(Control* c);
        void registerControls(std::initializer_list<Control*> cs);
        void update();
        void render();
        Control* getControlByID(int cid);

        virtual void onLoad() {}
        virtual void onUpdate() {}
        virtual void onRender() {}
        virtual void onLeaving() {}
        virtual void onLeave() {}

        void start();
        void singleloop();
    };
}
