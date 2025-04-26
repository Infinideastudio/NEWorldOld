#include "Globalization.h"

namespace Globalization {

	int count;
	string Cur_Lang = "zh_CN", Cur_Symbol = "", Cur_Name = "";
	map<int, Line> Lines;
	map<string, int> keys;

	bool LoadLang(string lang) {
		std::ifstream f("lang/" + lang + ".lang");
		if (!f.is_open()) return false;
		std::getline(f, Cur_Symbol);
		std::getline(f, Cur_Name);
		std::string line;
		Lines.clear();
		Cur_Lang = lang;
		for (int i = 0; i < count; i++) {
			getline(f, Lines[i].str);
		}
		return true;
	}

	bool Load() {
		std::ifstream f("lang/Keys.lk");
		if (!f.is_open()) return false;
		std::string line;
		keys.clear();
		count = 0;
		while (std::getline(f, line)) {
			if (line.empty()) break;
			keys[line] = count++;
		}
		return LoadLang(Cur_Lang);
	}

	string GetStrbyid(int id) {
		return Lines[id].str;
	}

	string GetStrbyKey(string key) {
		return Lines[keys[key]].str;
	}
}