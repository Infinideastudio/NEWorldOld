#include "Menus.h"

void saveoptions();

namespace Menus {
	class OptionsMenu :public GUI::Form {
	private:
		GUI::label title;
		GUI::trackbar FOVyBar, mmsBar, viewdistBar;
		GUI::button rdstbtn, gistbtn, backbtn, savebtn;
		void onLoad() {
			title = GUI::label("=================<  " + GetStrbyKey("gui:caption:Options") + "  >=================", -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			FOVyBar = GUI::trackbar(strWithVar(GetStrbyKey("gui:FOV"), FOVyNormal), 120, (int)(FOVyNormal - 1), -250, -10, 60, 84, 0.5, 0.5, 0.0, 0.0);
			mmsBar = GUI::trackbar(strWithVar(GetStrbyKey("gui:Mouse Sensitivity"), mousemove), 120, (int)(mousemove * 40 * 2 - 1), 10, 250, 60, 84, 0.5, 0.5, 0.0, 0.0);
			viewdistBar = GUI::trackbar(strWithVar(GetStrbyKey("gui:View Distance"), viewdistance), 120, (viewdistance - 2) * 4 - 1, -250, -10, 96, 120, 0.5, 0.5, 0.0, 0.0);
			rdstbtn = GUI::button(">> " + GetStrbyKey("gui:Render Options") + "...", -250, -10, 204, 228, 0.5, 0.5, 0.0, 0.0);
			gistbtn = GUI::button(">> " + GetStrbyKey("gui:GUI Options") + "...", 10, 250, 204, 228, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button("<< " + GetStrbyKey("gui:Back to Main") + "", -250, -10, -44, -20, 0.5, 0.5, 1.0, 1.0);
			savebtn = GUI::button(GetStrbyKey("gui:Save Options"), 10, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(8, &title, &FOVyBar, &mmsBar, &viewdistBar, &rdstbtn, &gistbtn, &backbtn, &savebtn);
		}
		void onUpdate() {
			FOVyNormal = (float)(FOVyBar.barpos + 1);
			mousemove = (mmsBar.barpos / 2 + 1) / 40.0f;
			viewdistance = (viewdistBar.barpos + 1) / 4 + 2;
			if (rdstbtn.clicked) Renderoptions();
			if (gistbtn.clicked) GUIoptions();
			if (backbtn.clicked) ExitSignal = true;
			if (savebtn.clicked) saveoptions();
			FOVyBar.text = strWithVar(GetStrbyKey("gui:FOV") + "£º", FOVyNormal);
			mmsBar.text = strWithVar(GetStrbyKey("gui:Mouse Sensitivity") + "£º", mousemove);
			viewdistBar.text = strWithVar(GetStrbyKey("gui:View Distance") + "£º", viewdistance);
		}
	};
	void options() { OptionsMenu Menu; Menu.start(); }
}
