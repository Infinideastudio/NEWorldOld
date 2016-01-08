#pragma once
#include"stdinclude.h"

namespace International {
	struct Line {
		string str;
		int id;
		GLuint PerRender;
	};
	
	extern map<int, Line> Lines;
	extern string Cur_Lang;
	extern string Yes, No, Enabled, Disabled;
	
	bool LoadLang(string lang);
	bool Load();
	string GetStrbyid(int id);
	string GetStrbyKey(string key);
}