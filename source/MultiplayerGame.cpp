#include "Menus.h"
#include "GUI.h"
#include "AudioSystem.h"

namespace Menus {
    int getDotCount(std::string s) {
        auto ret = 0;
        for (unsigned int i = 0; i != s.size(); i++)
            if (s[i] == '.') ret++;
        return ret;
    }

    class MultiplayerMenu : public GUI::Form {
    private:
        GUI::label title;
        GUI::textbox serveriptb;
        GUI::button runbtn, okbtn, backbtn;

        void onLoad() override {
            title = GUI::label("==============<  多 人 游 戏  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
            serveriptb = GUI::textbox("输入服务器IP", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
            runbtn = GUI::button("运行服务器（开服）", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
            okbtn = GUI::button("确定", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
            backbtn = GUI::button("<< 返回", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
            inputstr = "";
            okbtn.enabled = false;
            registerControls(4, &title, &serveriptb, &runbtn, &okbtn, &backbtn);
        }

        void onUpdate() override {
            static auto serveripChanged = false;
#ifdef NEWORLD_USE_WINAPI
            if (runbtn.clicked) WinExec("NEWorldServer.exe", SW_SHOWDEFAULT);
#endif
            if (okbtn.clicked) {
                serverip = serveriptb.text;
                gamebegin = true;
                multiplayer = true;
            }

            AudioSystem::SpeedOfSound = AudioSystem::Air_SpeedOfSound;
            //EFX::EAXprop = Generic;
            //EFX::UpdateEAXprop();
            float Pos[] = {0.0f, 0.0f, 0.0f};
            AudioSystem::Update(Pos, false, false, Pos, false, false);
            if (backbtn.clicked) GUI::PopPage();
            if (serveriptb.pressed && !serveripChanged) {
                serveriptb.text = "";
                serveripChanged = true;
            }
            if (serveriptb.text == "" || !serveripChanged || getDotCount(serveriptb.text) != 3) okbtn.enabled = false;
            else okbtn.enabled = true;
            inputstr = "";
        }
    };

    void multiplayermenu() { GUI::PushPage(new MultiplayerMenu); }
}
