#pragma once
#include "stdinclude.h"

namespace Globalization {
	struct Line {
		string str;
		int id;
		GLuint PerRender;
	};
	
	extern map<int, Line> Lines;
	extern map<string, int> keys;
	extern string Cur_Lang;
	extern string Yes, No, Enabled, Disabled;
	
	bool LoadLang(string lang);
	bool Load();
	string GetStrbyid(int id);
	string GetStrbyKey(string key);
}