#include "Menus.h"
#include "TextRenderer.h"

namespace Menus {
	class MainMenu :public GUI::Form {
	private:
		GUI::imagebox title;
		GUI::button startbtn, optionsbtn, quitbtn;
		GUI::label helplabel;
		void onLoad() {
			title = GUI::imagebox(0.0f, 1.0f, 0.0f, 1.0f, tex_title, -256, 256, 20, 276, 0.5, 0.5, 0.0, 0.0);
			startbtn = GUI::button(GetStrbyKey("NEWorld.main.start"), -200, 200, 280, 312, 0.5, 0.5, 0.0, 0.0);
			optionsbtn = GUI::button(GetStrbyKey("NEWorld.main.options"), -200, -3, 318, 352, 0.5, 0.5, 0.0, 0.0);
			quitbtn = GUI::button(GetStrbyKey("NEWorld.main.exit"), 3, 200, 318, 352, 0.5, 0.5, 0.0, 0.0);
			helplabel = GUI::label(GetStrbyKey("NEWorld.main.help"), 0, 0, -24, -8, 0.0, 1.0, 1.0, 1.0);
			registerControls(5, &title, &startbtn, &optionsbtn, &quitbtn, &helplabel);
		}
		void onUpdate() {
			if (startbtn.clicked) worldmenu();
			if (gamebegin) ExitSignal = true;
			if (optionsbtn.clicked) {
				options();
				startbtn.text = GetStrbyKey("NEWorld.main.start");
				optionsbtn.text = GetStrbyKey("NEWorld.main.options");
				quitbtn.text = GetStrbyKey("NEWorld.main.exit");
				helplabel.text = GetStrbyKey("NEWorld.main.help");
			}
			if (quitbtn.clicked) exit(0);
		}
	};
	void mainmenu() { MainMenu Menu; Menu.start(); }
}
