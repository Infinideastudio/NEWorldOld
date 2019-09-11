#pragma once

#include <cstdint>

bool QueryPerformanceCounter(int64_t* lpPerformanceCount) noexcept;
bool QueryPerformanceFrequency(int64_t* lpFrequency) noexcept;
