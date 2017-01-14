//#pragma warning(disable:4710) //忽略STL高发警告：函数未内联
//#pragma warning(disable:4514) //忽略STL高发警告：未使用的内联函数已移除
//#pragma warning(disable:4350) //忽略STL高发警告：行为更改
//#pragma warning(push,0) //忽略头文件的警告

#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <io.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <fstream>
#include <map>
#include <queue>
#include <functional>
#include <cassert>
#include <thread>
#include <mutex>
#include <random>
#include <unordered_set>

using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;
using std::map;
using std::cout;
using std::endl;

#define GLEW_STATIC
#include <GL/glew.h>
#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

//#pragma warning(pop)
//#pragma warning(disable:4820) //忽略不必要的警告：数据结构对齐
//#pragma warning(disable:4365) //忽略不必要的警告：有符号/无符号不匹配