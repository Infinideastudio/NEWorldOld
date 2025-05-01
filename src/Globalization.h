#pragma once
#include "StdInclude.h"

namespace Globalization {
extern std::string Cur_Lang;

bool LoadLang(std::string lang);
bool Load();
std::string GetStrbyid(int id);
std::string GetStrbyKey(std::string key);
}
