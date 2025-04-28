#pragma once
#define NEWORLD_USE_WINAPI
#define NERDMODE1
#ifdef NEWORLD_USE_WINAPI
	#ifdef NEWORLD_SERVER
	#include <thread>
	#include <mutex>
	using std::thread;
	using std::mutex;
	#endif
	#include <WinSock2.h>
	#include <Windows.h>
#else
	#include <thread>
	#include <mutex>
#endif
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <io.h>
#include <array>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <fstream>
#include <functional>
#include <algorithm>
#include <utility>
#include <optional>
#include <variant>
#include <tuple>
#include <cassert>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <queue>
#include <stdarg.h>
#include <direct.h>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;
using std::map;
using std::cout;
using std::endl;
using std::max;
using std::min;

#ifdef NEWORLD_GAME
// GLEW
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>
// FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif
