module;

#include <cmath>
#include <string>
#include <sstream>

module menus;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class RenderOptionsMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::Button smoothlightingbtn = GUI::Button("", -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Button fancygrassbtn = GUI::Button("", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Button mergefacebtn = GUI::Button("", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar msaabar = GUI::Trackbar(
        "",
        120,
        Multisample == 0 ? 0 : (int) (std::log2(Multisample) - 1) * 40 - 1,
        10,
        250,
        96,
        120,
        0.5,
        0.5,
        0.0,
        0.0
    );
    GUI::Button vsyncbtn = GUI::Button("", -250, -10, 132, 156, 0.5, 0.5, 0.0, 0.0);
    GUI::Button shaderbtn = GUI::Button("", 10, 250, 132, 156, 0.5, 0.5, 0.0, 0.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() {
        title.centered = true;
        registerControls(
            {&title, &smoothlightingbtn, &fancygrassbtn, &mergefacebtn, &msaabar, &vsyncbtn, &shaderbtn, &backbtn}
        );
    }

    void onUpdate() {
        title.text = GetStrbyKey("NEWorld.render.caption");
        smoothlightingbtn.text = GetStrbyKey("NEWorld.render.smooth") + BoolEnabled(SmoothLighting);
        fancygrassbtn.text = GetStrbyKey("NEWorld.render.grasstex") + BoolYesNo(NiceGrass);
        mergefacebtn.text = GetStrbyKey("NEWorld.render.merge") + BoolEnabled(MergeFace);
        msaabar.text = GetStrbyKey("NEWorld.render.multisample")
                     + (Multisample != 0 ? Var2Str(Multisample) + "x" : BoolEnabled(false));
        vsyncbtn.text = GetStrbyKey("NEWorld.render.vsync") + BoolEnabled(VerticalSync);
        shaderbtn.text = GetStrbyKey("NEWorld.render.shaders");
        backbtn.text = GetStrbyKey("NEWorld.render.back");

        if (smoothlightingbtn.clicked)
            SmoothLighting = !SmoothLighting;
        if (fancygrassbtn.clicked)
            NiceGrass = !NiceGrass;
        if (mergefacebtn.clicked)
            MergeFace = !MergeFace;
        if (msaabar.barpos == 0)
            Multisample = 0;
        else
            Multisample = 1 << ((msaabar.barpos + 1) / 40 + 1);
        if (vsyncbtn.clicked)
            VerticalSync = !VerticalSync;
        if (shaderbtn.clicked)
            shaderoptions();
        if (backbtn.clicked)
            exit = true;
    }
};

void renderoptions() {
    RenderOptionsMenu Menu;
    Menu.start();
}
}
