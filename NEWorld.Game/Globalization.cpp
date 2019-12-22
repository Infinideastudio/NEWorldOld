#include <fstream>
#include "Globalization.h"
#include <map>
#include <nlohmann/json.hpp>
#include <iostream>

namespace Globalization {

    int count;
    std::string Cur_Lang = "zh_CN", Cur_Symbol = "", Cur_Name = "";
    std::map<int, Line> Lines;
    std::map<std::string, int> keys;

    bool LoadLang(const std::string& lang) {
        std::ifstream f("./Assets/Translations/"+lang+".lang");
        if (f.bad()) {
            return false;
        }
        Lines.clear();
        Cur_Lang = lang;
        getline(f, Cur_Symbol);
        getline(f, Cur_Name);
        for (auto i = 0; i<count; i++) {
            getline(f, Lines[i].str);
        }
        f.close();
        return true;
    }

    bool Load() {
        std::ifstream f("./Assets/Translations/Keys.json");
        if (f.good()) {
            try {
                nlohmann::json file{};
                f >> file;
                auto i = 0;
                for (auto& v: file) { keys[v] = i++; }
                count = i;
            }
            catch (...) {
                return false;
            }
            return LoadLang(Cur_Lang);
        }
        return false;
    }

    std::string GetStrbyid(int id) {
        return Lines[id].str;
    }

    std::string GetStrbyKey(std::string key) {
        return Lines[keys[key]].str;
    }
}
