#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <utf8/cpp20.h>

std::vector<std::string> split(std::string str, std::string pattern);

extern unsigned int g_seed;
inline int fastRand() {
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}
inline void fastSrand(int seed) {
    g_seed = seed;
}
inline double rnd() {
    return (double) fastRand() / (RAND_MAX + 1);
}

inline void DebugInfo(std::string_view msg) {
    std::cerr << "[INFO] " << msg << std::endl;
}

inline void DebugWarning(std::string_view msg) {
    std::cerr << "[WARN] " << msg << std::endl;
}

inline void DebugError(std::string_view msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

inline double Timer() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void Sleep(unsigned int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline std::u32string UTF8Unicode(std::string_view s) {
    return utf8::utf8to32(s);
}

inline std::string UnicodeUTF8(std::u32string_view s) {
    return utf8::utf32to8(s);
}
