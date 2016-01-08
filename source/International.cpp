#include"International.h"
using namespace std;
namespace NEInternational {

	string Cur_Lang = "CHS", Cur_Symbol = "", Cur_Name = "";
	string Yes, No, Enabled, Disabled;
	map<int, Line> Lines;

	bool LoadLang(string lang)
	{
		ifstream f("Textures/Lang/" + lang + ".lang");
		if (f.bad()) { 
			return false;
		};
		int pos;
		int count;
		Cur_Lang = lang;
		f >> Cur_Symbol; f.get();
		getline(f, Cur_Name);
		getline(f, Yes);
		getline(f, No);
		getline(f, Enabled);
		getline(f, Disabled);
		f >> pos;
		f >> count;
		f.get();
		for (int i = 0; i < count; ++i) {
			getline(f, Lines[pos].str);
			++pos;
		}
		f.close();
		return true;
	}

	bool Load()
	{
		return LoadLang(Cur_Lang);
	}

	string GetStr(int id)
	{
		return Lines[id].str;
	}
}