#pragma once
#include "StdInclude.h"

namespace Globalization {
	struct Line { std::string str; };
	
	extern int count;
	extern std::map<int, Line> Lines;
	extern std::map<std::string, int> keys;
	extern std::string Cur_Lang;
	
	bool LoadLang(std::string lang);
	bool Load();
	std::string GetStrbyid(int id);
	std::string GetStrbyKey(std::string key);
}
