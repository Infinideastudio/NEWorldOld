#pragma once
#include "StdInclude.h"
#include "Typedefs.h"

#include <codecvt> // TODO: find a replacement?

std::vector<std::string> split(std::string str, std::string pattern);

extern unsigned int g_seed;
inline int fastRand() {
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}
inline void fastSrand(int seed) { g_seed = seed; }
inline double rnd() { return (double)fastRand() / (RAND_MAX + 1); }

inline void DebugInfo(std::string_view msg) {
    std::cerr << "[INFO] " << msg << std::endl;
}

inline void DebugWarning(std::string_view msg) {
    std::cerr << "[WARN] " << msg << std::endl;
}

inline void DebugError(std::string_view msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

inline double Timer() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline void Sleep(unsigned int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

inline std::u32string UTF8Unicode(std::string const& s) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.from_bytes(s);
}

inline std::string UnicodeUTF8(std::u32string const& s) {
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    return converter.to_bytes(s);
}
