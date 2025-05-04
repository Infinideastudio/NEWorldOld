module menus;
import std;
import types;
import gui;
import globals;
import globalization;

namespace Menus {
using Globalization::GetStrbyKey;

class Language: public GUI::Form {
private:
    struct LangInfo {
        std::string Symbol, EngSymbol, Name;
        std::unique_ptr<GUI::Button> Button;
    };
    std::vector<LangInfo> langs;
    GUI::Label title = GUI::Label("", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
    GUI::Button backbtn = GUI::Button("", -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);

    void onLoad() override {
        title.centered = true;
        registerControls({&title, &backbtn});

        std::ifstream index("lang/langs.txt");
        std::string line;
        int count = 0;
        while (std::getline(index, line)) {
            if (line.empty())
                break;
            LangInfo info;
            info.Symbol = line;
            std::ifstream LF("lang/" + info.Symbol + ".lang");
            std::getline(LF, info.EngSymbol);
            std::getline(LF, info.Name);
            LF.close();
            info.Button = std::make_unique<
                GUI::Button>(info.Name, -200, 200, count * 36 + 60, count * 36 + 90, 0.5, 0.5, 0.0, 0.0);
            registerControl(info.Button.get());
            langs.emplace_back(std::move(info));
            count++;
        }
    }

    void onUpdate() override {
        title.text = GetStrbyKey("NEWorld.language.caption");
        backbtn.text = GetStrbyKey("NEWorld.language.back");

        if (backbtn.clicked)
            exit = true;
        for (size_t i = 0; i < langs.size(); i++) {
            if (langs[i].Button->clicked) {
                exit = true;
                if (Cur_Lang != langs[i].Symbol) {
                    Globalization::LoadLang(langs[i].Symbol);
                }
                break;
            }
        }
    }
};

void languagemenu() {
    Language Menu;
    Menu.start();
}
}
