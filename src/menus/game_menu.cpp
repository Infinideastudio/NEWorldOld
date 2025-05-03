module;

module menus;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class GameMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, -76, -60, 0.5, 0.5, 0.5, 0.5);
    GUI::Button resumebtn = GUI::Button("", -200, 200, -36, -3, 0.5, 0.5, 0.5, 0.5);
    GUI::Button exitbtn = GUI::Button("", -200, 200, 3, 36, 0.5, 0.5, 0.5, 0.5);

    void onLoad() override {
        title.centered = true;
        registerControls({&title, &resumebtn, &exitbtn});
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.pause.caption");
        resumebtn.text = GetStrbyKey("NEWorld.pause.continue");
        exitbtn.text = GetStrbyKey("NEWorld.pause.back");

        if (resumebtn.clicked)
            exit = true;
        if (exitbtn.clicked)
            GameExit = exit = true;
    }
};

void gamemenu() {
    GameMenu Menu;
    Menu.start();
}
}
