module menus;
import std;
import types;
import gui;
import globals;
import globalization;
import textures;

namespace Menus {
using Globalization::GetStrbyKey;

class MainMenu: public GUI::Form {
private:
    GUI::ImageBox title = GUI::ImageBox(0.0f, 1.0f, 0.0f, 1.0f, TitleTexture, -256, 256, 20, 276, 0.5, 0.5, 0.0, 0.0);
    GUI::Button startbtn = GUI::Button("", -200, 200, 280, 312, 0.5, 0.5, 0.0, 0.0);
    GUI::Button optionsbtn = GUI::Button("", -200, -3, 318, 352, 0.5, 0.5, 0.0, 0.0);
    GUI::Button quitbtn = GUI::Button("", 3, 200, 318, 352, 0.5, 0.5, 0.0, 0.0);
    GUI::Label helplabel = GUI::Label("", 0, 0, -24, -8, 0.0, 1.0, 1.0, 1.0);

    void onLoad() override {
        registerControls({&title, &startbtn, &optionsbtn, &quitbtn, &helplabel});
    }

    void onUpdate() override {
        startbtn.text = GetStrbyKey("NEWorld.main.start");
        optionsbtn.text = GetStrbyKey("NEWorld.main.options");
        quitbtn.text = GetStrbyKey("NEWorld.main.exit");
        helplabel.text = GetStrbyKey("NEWorld.main.help");

        if (startbtn.clicked)
            worldmenu();
        if (GameBegin)
            exit = true;
        if (optionsbtn.clicked)
            options();
        if (quitbtn.clicked)
            std::exit(0);
    }
};

void mainmenu() {
    MainMenu Menu;
    Menu.start();
}
}
