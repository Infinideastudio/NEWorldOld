#pragma once
#include "StdInclude.h"

namespace Globalization {
	struct Line {
		std::string str;
		int id;
	};
	
	extern int count;
	extern map<int, Line> Lines;
	extern map<std::string, int> keys;
	extern std::string Cur_Lang;
	
	bool LoadLang(std::string lang);
	bool Load();
	std::string GetStrbyid(int id);
	std::string GetStrbyKey(std::string key);
}