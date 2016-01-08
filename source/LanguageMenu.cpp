#include "Menus.h"

namespace Menus {
	class Language :public GUI::Form {
	private:
		GUI::label title;
		GUI::button resumebtn, exitbtn;
		void onLoad() {
			title = GUI::label("==============<  多 国 语 言  >==============", -225, 225, 0, 16, 0.5, 0.5, 0.25, 0.25);
			resumebtn = GUI::button("继续游戏", -200, 200, -35, -3, 0.5, 0.5, 0.5, 0.5);
			exitbtn = GUI::button("<< 返回主菜单", -200, 200, 3, 35, 0.5, 0.5, 0.5, 0.5);
			registerControls(3, &title, &resumebtn, &exitbtn);
		}
		void onUpdate() {
			if (resumebtn.clicked) ExitSignal = true;
			if (exitbtn.clicked) gameexit = ExitSignal = true;
		}
	};
	void languagemenu() { Language Menu; Menu.start(); }
}
