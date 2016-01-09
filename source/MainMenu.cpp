#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class MainMenu :public GUI::Form {
	private:
		GUI::imagebox title;
		GUI::button startbtn, optionsbtn, quitbtn;
		void onLoad() {
			title = GUI::imagebox(0.0f, 1.0f, 0.5f, 1.0f, tex_title, -256, 256, 20, 276, 0.5, 0.5, 0.0, 0.0);
			startbtn = GUI::button(GetStrbyKey("gui.main.start"), -200, 200, 280, 312, 0.5, 0.5, 0.0, 0.0);
			optionsbtn = GUI::button(GetStrbyKey("gui.main.options"), -200, -3, 318, 352, 0.5, 0.5, 0.0, 0.0);
			quitbtn = GUI::button(GetStrbyKey("gui.main.exit"), 3, 200, 318, 352, 0.5, 0.5, 0.0, 0.0);
			registerControls(4, &title, &startbtn, &optionsbtn, &quitbtn);
		}
		void onUpdate() {
			if (startbtn.clicked) worldmenu();
			if (gamebegin) ExitSignal = true;
			if (optionsbtn.clicked) options();
			if (quitbtn.clicked) exit(0);
		}
	};
	void mainmenu() { MainMenu Menu; Menu.start(); }
}
