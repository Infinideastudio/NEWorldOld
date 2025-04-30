#include "Menus.h"
#include "World.h"

namespace Menus {
class CreateWorldMenu: public GUI::Form {
private:
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::TextBox worldnametb = GUI::TextBox("", -250, 250, 48, 72, 0.5, 0.5, 0.0, 0.0);
    GUI::Button okbtn = GUI::Button("", -250, 250, 84, 120, 0.5, 0.5, 0.0, 0.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() {
        title.centered = true;
        okbtn.enabled = false;
        registerControls({&title, &worldnametb, &okbtn, &backbtn});
    }

    void onUpdate() {
        title.text = GetStrbyKey("NEWorld.create.caption");
        worldnametb.text = GetStrbyKey("NEWorld.create.inputname");
        okbtn.text = GetStrbyKey("NEWorld.create.ok");
        backbtn.text = GetStrbyKey("NEWorld.create.back");

        okbtn.enabled = !worldnametb.input.empty();
        if (okbtn.clicked) {
            if (!worldnametb.input.empty()) {
                World::WorldName = UnicodeUTF8(worldnametb.input);
                GameBegin = true;
            }
            exit = true;
        }
        if (backbtn.clicked)
            exit = true;
    }
};

void createworldmenu() {
    CreateWorldMenu Menu;
    Menu.start();
}
}
