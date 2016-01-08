#include"International.h"
using namespace std;
namespace NEInternational {

	string Cur_Lang = "", Cur_Symbol = "", Cur_Name = "";
	map<int, Line> Lines;

	bool LoadLang(string lang)
	{
		ifstream f("Textures/Lang/" + lang + ".lang");
		if (f.bad()) { return false; };
		int pos;
		Cur_Lang = lang;
		f >> Cur_Symbol;
		f >> Cur_Name;
		f >> pos;
		while (!f.eof) {
			getline(f, Lines[pos].str);
			++pos;
		}
		f.close();
		return true;
	}

	string GetStr(int id)
	{
		return Lines[id].str;
	}
}