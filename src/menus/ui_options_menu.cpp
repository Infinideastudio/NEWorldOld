module menus;
import std;
import types;
import gui;
import globals;
import globalization;
import setup;
import text_rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class UIOptionsMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar fontbar = GUI::Trackbar("", 120, (FontScale - 0.5) * 120 - 1, -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Button blurbtn = GUI::Button("", 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Button ppistretchbtn = GUI::Button("", -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() override {
        title.centered = true;
        registerControls({&title, &fontbar, &blurbtn, &ppistretchbtn, &backbtn});
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.gui.caption");
        fontbar.text = GetStrbyKey("NEWorld.gui.fontsize") + Var2Str(FontScale);
        blurbtn.text = GetStrbyKey("NEWorld.gui.blur") + BoolEnabled(UIBackgroundBlur);
        ppistretchbtn.text = GetStrbyKey("NEWorld.gui.stretch") + BoolEnabled(UIAutoStretch);
        backbtn.text = GetStrbyKey("NEWorld.gui.back");

        FontScale = (fontbar.barpos + 1) / 120.0 + 0.5;
        if (fontbar.pressed)
            TextRenderer::init_font(true);
        if (blurbtn.clicked)
            UIBackgroundBlur = !UIBackgroundBlur;
        if (ppistretchbtn.clicked)
            toggle_stretch();
        if (backbtn.clicked) {
            exit = true;
        }
    }
};

void uioptions() {
    UIOptionsMenu Menu;
    Menu.start();
}
}
