#include "Menus.h"
#include <deque>
struct Langinfo {
	string Symbol, EngSymbol, Name;
	GUI::button * Button;
};

namespace Menus {
	class Language :public GUI::Form {
	private:
		std::deque<Langinfo> Langs;
		GUI::label title;
		GUI::button backbtn;

		void onLoad() {
			Langs.clear();
			title = GUI::label(GetStrbyKey("NEWorld.language.caption"), -225, 225, 20, 36, 0.5, 0.5, 0.0, 0.0);
			backbtn = GUI::button(GetStrbyKey("NEWorld.language.back"), -250, 250, -44, -20, 0.5, 0.5, 1.0, 1.0);
			registerControls(2, &title, &backbtn); 
			std::ifstream index("lang/langs.txt");
			std::string line;
			int count = 0;
			while (std::getline(index, line)) {
				if (line.empty()) break;
				Langinfo info;
				info.Symbol = line;
				std::ifstream LF("lang/" + info.Symbol + ".lang");
				std::getline(LF, info.EngSymbol);
				std::getline(LF, info.Name);
				LF.close();
				info.Button = new GUI::button(info.EngSymbol + " -- " + info.Name, -200, 200, count * 36 + 42, count * 36 + 72, 0.5, 0.5, 0.0, 0.0);
				registerControls(1, info.Button);
				Langs.push_back(info);
				count++;
			}
		}

		void onUpdate() {
			if (backbtn.clicked) ExitSignal = true;
			for (size_t i = 0; i < Langs.size(); i++) {
				if (Langs[i].Button->clicked){
					ExitSignal = true;
					if (Globalization::Cur_Lang != Langs[i].Symbol) {
						Globalization::LoadLang(Langs[i].Symbol);
					}
					break;
				}
			}
		}

		void onLeave() {
			for (size_t i = 0; i < Langs.size(); i++) {
				for (vector<GUI::controls*>::iterator iter = children.begin(); iter != children.end(); ) {
					if ((*iter)->id == Langs[i].Button->id) iter = children.erase(iter);
					else ++iter;
				}
				Langs[i].Button->destroy();
				delete Langs[i].Button;
			}
		}
	};
	void languagemenu() { Language Menu; Menu.start(); }
}
