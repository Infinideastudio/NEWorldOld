#pragma once
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <ctime>
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
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <queue>
#include <numbers>
#include <random>

#ifdef NEWORLD_USE_WINAPI
#ifdef NEWORLD_SERVER
#include <thread>
#include <mutex>
using std::thread;
using std::mutex;
#endif
#define NOMINMAX
#include <Windows.h>
#undef near
#undef far
#else
#endif

#ifdef NEWORLD_GAME
// GLEW
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>
#endif

using std::string;
using std::vector;
