#pragma once
#include"stdinclude.h"

namespace NEInternational {
	struct Line {
		string str;
		int id;
		GLuint PerRender;
	};
	
	extern map<int, Line> Lines;
	
	bool LoadLang(string lang);
	string GetStr(int id);
}