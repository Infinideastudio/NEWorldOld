#include "Menus.h"

namespace Menus {
	class Language :public GUI::Form {
	private:
		vector<string> name, idtf;
		GUI::label title;
		GUI::button resumebtn, exitbtn;

		void onLoad() {
			title = GUI::label("==============<  " + GetStrbyKey("gui:caption:Multilanguage") + "  >==============", -225, 225, 0, 16, 0.5, 0.5, 0.25, 0.25);
			resumebtn = GUI::button("继续游戏", -200, 200, -35, -3, 0.5, 0.5, 0.5, 0.5);
			exitbtn = GUI::button("<<  "+ GetStrbyKey("gui:Back") , -200, 200, 3, 35, 0.5, 0.5, 0.5, 0.5);
			registerControls(3, &title, &resumebtn, &exitbtn); 
			//查找所有世界存档
			long hFile = 0;
			_finddata_t fileinfo;
			if ((hFile = _findfirst(string("Worlds\\*").c_str(), &fileinfo)) != -1) {
				do {
					if ((fileinfo.attrib & _A_NORMAL)) {
						
					}
				} while (_findnext(hFile, &fileinfo) == 0);
				_findclose(hFile);
			}

		}
		void onUpdate() {
			if (resumebtn.clicked) ExitSignal = true;
			if (exitbtn.clicked) gameexit = ExitSignal = true;
		}
	};
	void languagemenu() { Language Menu; Menu.start(); }
}
