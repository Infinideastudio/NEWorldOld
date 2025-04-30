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

inline void DebugInfo(std::string msg) {
#ifdef NEWORLD_USE_WINAPI
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
	printf("[Debug][Info]");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	printf("%s\n", msg.c_str());
#else
	printf("[Debug][Info]%s\n", msg.c_str());
#endif
}

inline void DebugWarning(std::string msg) {
#ifdef NEWORLD_USE_WINAPI
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
	printf("[Debug][Warning]");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	printf("%s\n", msg.c_str());
#else
	printf("[Debug][Warning]%s\n", msg.c_str());
#endif
}

inline void DebugError(std::string msg) {
#ifdef NEWORLD_USE_WINAPI
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
	printf("[Debug][Error]");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	printf("%s\n", msg.c_str());
#else
	printf("[Debug][Error]%s\n", msg.c_str());
#endif
}

#ifdef NEWORLD_USE_WINAPI
inline double timer() {
	static LARGE_INTEGER counterFreq;
	if (counterFreq.QuadPart == 0) QueryPerformanceFrequency(&counterFreq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart / counterFreq.QuadPart;
}
#else
inline void Sleep(unsigned int ms) { unsigned int fr = clock(); while (clock() - fr <= ms); }
inline double timer() { return (double)clock() / CLOCKS_PER_SEC; }
#endif

inline std::u32string UTF8Unicode(std::string const& s) {
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
	return converter.from_bytes(s);
}

inline std::string UnicodeUTF8(std::u32string const& s) {
	std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
	return converter.to_bytes(s);
}
