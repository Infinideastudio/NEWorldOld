module menus;
import std;
import types;
import gui;
import globals;
import globalization;
import rendering;

namespace Menus {
using Globalization::GetStrbyKey;

class OptionsMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);

    GUI::Trackbar fovbar = GUI::Trackbar("", 120, (int) (FOVyNormal - 1), -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar mmsbar =
        GUI::Trackbar("", 120, (int) (MouseSpeed * 160 * 2 - 1), 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
    GUI::Trackbar viewdistbar =
        GUI::Trackbar("", 120, (RenderDistance - 4) * 2 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);

    GUI::Button rdstbtn = GUI::Button("", -250, -10, 204, 228, 0.5, 0.5, 0.0, 0.0);
    GUI::Button uistbtn = GUI::Button("", 10, 250, 204, 228, 0.5, 0.5, 0.0, 0.0);
    GUI::Button langbtn = GUI::Button("", -250, -10, 240, 264, 0.5, 0.5, 0.0, 0.0);

    GUI::Button backbtn = GUI::Button("", -250, -10, -44, -20, 0.5, 0.5, 1.0, 1.0);
    GUI::Button savebtn = GUI::Button("", 10, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() override {
        title.centered = true;
        registerControls({&title, &fovbar, &mmsbar, &viewdistbar, &rdstbtn, &uistbtn, &langbtn, &backbtn, &savebtn});
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.options.caption");
        fovbar.text = strWithVar(GetStrbyKey("NEWorld.options.fov"), FOVyNormal);
        mmsbar.text = strWithVar(GetStrbyKey("NEWorld.options.sensitivity"), MouseSpeed);
        viewdistbar.text = strWithVar(GetStrbyKey("NEWorld.options.distance"), RenderDistance);
        rdstbtn.text = GetStrbyKey("NEWorld.options.rendermenu");
        uistbtn.text = GetStrbyKey("NEWorld.options.guimenu");
        langbtn.text = GetStrbyKey("NEWorld.options.languagemenu");
        backbtn.text = GetStrbyKey("NEWorld.options.back");
        savebtn.text = GetStrbyKey("NEWorld.options.save");

        FOVyNormal = (float) (fovbar.barpos + 1);
        MouseSpeed = (mmsbar.barpos / 2 + 1) / 160.0f;
        RenderDistance = (viewdistbar.barpos + 1) / 2 + 4;
        if (rdstbtn.clicked)
            renderoptions();
        if (uistbtn.clicked)
            uioptions();
        if (backbtn.clicked) {
            Renderer::init_shaders(true);
            exit = true;
        }
        if (savebtn.clicked)
            save_options();
        if (langbtn.clicked)
            languagemenu();
    }
};

void options() {
    OptionsMenu Menu;
    Menu.start();
}
}
