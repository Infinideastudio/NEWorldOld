#include "Menus.h"

namespace Menus {
	int getDotCount(string s) {
		int ret = 0;
		for (unsigned int i = 0; i != s.size(); i++)
			if (s[i] == '.') ret++;
		return ret;
	}

	class MultiplayerMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::textbox serveriptb;
		GUI::button runbtn, okbtn, backbtn;
		void onLoad() {
			title = GUI::label("==============<  �� �� �� Ϸ  >==============", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			serveriptb = GUI::textbox("���������IP", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			runbtn = GUI::button("���з�������������", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			okbtn = GUI::button("ȷ��", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< ����", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			inputstr = "";
			okbtn.enabled = false;
			registerControls(4, &title, &serveriptb, &runbtn, &okbtn, &backbtn);
		}
		void onUpdate() {
			static bool serveripChanged = false;
#ifdef NEWORLD_USE_WINAPI
			if (runbtn.clicked) WinExec("NEWorldServer.exe", SW_SHOWDEFAULT);
#endif
			if (okbtn.clicked) {
				serverip = serveriptb.text;
				gamebegin = true;
				multiplayer = true;
			}
			if (backbtn.clicked) ExitSignal = true;
			if (serveriptb.pressed && !serveripChanged) {
				serveriptb.text = "";
				serveripChanged = true;
			}
			if (serveriptb.text == "" || !serveripChanged || getDotCount(serveriptb.text) != 3) okbtn.enabled = false;
			else okbtn.enabled = true;
			inputstr = "";
		}
	};
	void multiplayermenu() { MultiplayerMenu Menu; Menu.start(); }
}