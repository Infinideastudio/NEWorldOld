#pragma once

#include "stdinclude.h"
#include "Typedefs.h"
#include <chrono>
#include <vector>
#include <sstream>
#if __has_include(<Windows.h>)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NEWORLD_WIN32
#include <Windows.h>
#endif

extern double stretch;

//常用函数
std::vector<std::string> split(const std::string &str, const std::string &pattern);

inline void UITrans(double x, double y) {
    glTranslated(x * stretch, y * stretch, 0);
}

inline void UITrans(int x, int y) {
    glTranslated((static_cast<double>(x)) * stretch, (static_cast<double>(y)) * stretch, 0);
}

inline void UIVertex(double x, double y) {
    glVertex2d(x * stretch, y * stretch);
}

inline void UIVertex(int x, int y) {
    glVertex2i(static_cast<int>(x * stretch), static_cast<int>(y * stretch));
}

extern unsigned int g_seed;

inline int fastRand() {
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}

inline void fastSrand(int seed) { g_seed = seed; }

std::vector<std::string> split(const std::string &str, const std::string &pattern);

inline std::string boolstr(bool b) { return b ? "True" : "False"; }

inline double rnd() { return (double) fastRand() / (RAND_MAX + 1); }

inline int RoundInt(double d) { return int(floor(d + 0.5)); }

inline std::string itos(int i) {
    std::stringstream ss;
    ss << i;
    return ss.str();
}

inline bool beginWith(const std::string &str, const std::string &begin) {
    if (str.size() < begin.size()) return false;
    return str.substr(0, begin.size()) == begin;
}

inline void DebugWarning(const std::string &msg) {
#ifdef NEWORLD_USE_WINAPI
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
    printf("[Debug][Warning]");
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    printf("%s\n", msg.c_str());
#else
    printf("[Debug][Warning]%s\n", msg.c_str());
#endif
}

inline void DebugError(const std::string &msg) {
#ifdef NEWORLD_USE_WINAPI
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
    printf("[Debug][Error]");
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    printf("%s\n", msg.c_str());
#else
    printf("[Debug][Error]%s\n", msg.c_str());
#endif
}

template<class T>
inline void conv(const std::string &str, T &ret) {
    std::stringstream s(str);
    s >> ret;
}

template<class T>
inline T clamp(T x, T min, T max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

inline Mutex_t MutexCreate() { return new std::mutex; }

inline void MutexDestroy(Mutex_t _hMutex) { delete _hMutex; }

inline void MutexLock(Mutex_t _hMutex) { _hMutex->lock(); }

inline void MutexUnlock(Mutex_t _hMutex) { _hMutex->unlock(); }

inline Thread_t ThreadCreate(ThreadFunc_t func, void *param) { return new std::thread([=] { func(param); }); }

inline void ThreadWait(Thread_t _hThread) { _hThread->join(); }

inline void ThreadDestroy(Thread_t _hThread) { delete _hThread; }

inline unsigned int MByteToWChar(wchar_t *dst, const char *src, unsigned int n) {
#ifdef NEWORLD_WIN32
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, n, dst, n);
#else
    return mbstowcs(dst, src, n);
#endif
}

inline unsigned int WCharToMByte(char *dst, const wchar_t *src, unsigned int n) {
#ifdef NEWORLD_WIN32
    return WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, src, n, dst, n, nullptr, nullptr);
#else
    return wcstombs(dst, src, n);
#endif
}

inline unsigned int wstrlen(const wchar_t *wstr) { return wcslen(wstr); }

inline void SleepMs(unsigned int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

inline double timer() { return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count()) / 1000.0;
}

inline int DistanceSquare(int ix, int iy, int iz, int x, int y, int z)//计算距离的平方
{
    return (ix - x) * (ix - x) + (iy - y) * (iy - y) + (iz - z) * (iz - z);
}
