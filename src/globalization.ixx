export module globalization;
import std;
import types;
import globals;

namespace Globalization {

struct Line {
    std::string str;
};
int count;
std::string Cur_Symbol = "", Cur_Name = "";
std::map<int, Line> Lines;
std::map<std::string, int> keys;

export auto LoadLang(std::string const& lang) -> bool {
    auto f = std::ifstream("lang/" + lang + ".lang");
    if (!f.is_open())
        return false;
    std::getline(f, Cur_Symbol);
    std::getline(f, Cur_Name);
    std::string line;
    Lines.clear();
    Cur_Lang = lang;
    int i = 0;
    while (std::getline(f, line)) {
        if (line.empty())
            break;
        Lines[i++].str = line;
    }
    return true;
}

export auto Load() -> bool {
    auto f = std::ifstream("lang/Keys.lk");
    if (!f.is_open())
        return false;
    std::string line;
    keys.clear();
    count = 0;
    while (std::getline(f, line)) {
        if (line.empty())
            break;
        keys[line] = count++;
    }
    return LoadLang(Cur_Lang);
}

export auto GetStrbyid(int id) -> std::string {
    return Lines[id].str;
}

export auto GetStrbyKey(std::string const& key) -> std::string {
    return Lines[keys[key]].str;
}
}

export std::string BoolYesNo(bool b) {
    return b ? Globalization::GetStrbyKey("NEWorld.yes") : Globalization::GetStrbyKey("NEWorld.no");
}

export std::string BoolEnabled(bool b) {
    return b ? Globalization::GetStrbyKey("NEWorld.enabled") : Globalization::GetStrbyKey("NEWorld.disabled");
}

export template <typename T>
std::string strWithVar(std::string const& str, T var) {
    std::stringstream ss;
    ss << str << var;
    return ss.str();
}

export template <typename T>
std::string Var2Str(T var) {
    std::stringstream ss;
    ss << std::setprecision(2) << var;
    return ss.str();
}
