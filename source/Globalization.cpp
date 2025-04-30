#include "Globalization.h"

namespace Globalization {

    int count;
    std::string Cur_Lang = "zh_CN", Cur_Symbol = "", Cur_Name = "";
    std::map<int, Line> Lines;
    std::map<std::string, int> keys;

    bool LoadLang(std::string lang) {
        std::ifstream f("lang/" + lang + ".lang");
        if (!f.is_open()) return false;
        std::getline(f, Cur_Symbol);
        std::getline(f, Cur_Name);
        std::string line;
        Lines.clear();
        Cur_Lang = lang;
        int i = 0;
        while (std::getline(f, line)) {
            if (line.empty()) break;
            Lines[i++].str = line;
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

    std::string GetStrbyid(int id) {
        return Lines[id].str;
    }

    std::string GetStrbyKey(std::string key) {
        return Lines[keys[key]].str;
    }
}