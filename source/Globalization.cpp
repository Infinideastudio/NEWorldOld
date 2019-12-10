#include <fstream>
#include "Globalization.h"

namespace Globalization {

	int count;
	std::string Cur_Lang = "zh_CN", Cur_Symbol = "", Cur_Name = "";
    std::map<int, Line> Lines;
    std::map<std::string, int> keys;

	bool LoadLang(std::string lang) {
		std::ifstream f("Lang/" + lang + ".lang");
		if (f.bad()) {
			exit(-101);
			return false;
		}
		Lines.clear();
		Cur_Lang = lang;
		f >> Cur_Symbol; f.get();
		getline(f, Cur_Name);
		for (int i = 0; i < count; i++) {
			getline(f, Lines[i].str);
		}
		f.close();
		return true;
	}

	bool Load() {
		std::ifstream f("Lang/Keys.lk");
		if (f.bad()) return false;
		f >> count; f.get();
		for (int i = 0; i < count; i++) {
			std::string temp;
			getline(f, temp);
			keys[temp] = i;
		}
		f.close();
		return LoadLang(Cur_Lang);
	}

	std::string GetStrbyid(int id) {
		return Lines[id].str;
	}

	std::string GetStrbyKey(std::string key) {
		return Lines[keys[key]].str;
	}
}