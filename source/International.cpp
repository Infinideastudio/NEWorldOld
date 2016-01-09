#include"International.h"
using namespace std;
namespace International {

	string Cur_Lang = "CHS", Cur_Symbol = "", Cur_Name = "";
	string Yes, No, Enabled, Disabled;
	map<int, Line> Lines;
	map<string, int>keys;

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
		ifstream f("Textures/Lang/keys.lk");
		if (f.bad()) {
			return false;
		};
		int pos;
		int count;
		f >> pos;
		f >> count;
		f.get();
		for (int i = 0; i < count; ++i) {
			string temp;
			getline(f, temp);
			keys[temp] = pos;
			++pos;
		}
		f.close();
		return LoadLang(Cur_Lang);
	}

	string GetStrbyid(int id)
	{
		return Lines[id].str;
	}
	string GetStrbyKey(string key)
	{
		return Lines[keys[key]].str;
	}
}