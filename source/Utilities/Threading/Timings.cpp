#include "Timings.h"

#if __has_include(<mach/mach_time.h>)
#define HAVE_MACH_ABSOLUTE_TIME 1
#include <mach/mach_time.h>
#endif

#if __has_include(<time.h>)
#include <time.h>
#if defined(CLOCK_MONOTONIC)
#define HAVE_CLOCK_MONOTONIC 1
#endif
#endif

#if __has_include(<Windows.h>)
#define _WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define HAS_WIN32_CLOCK 1
#endif

namespace {
    constexpr int tccSecondsToNanoSeconds = 1000000000;

#if HAVE_MACH_ABSOLUTE_TIME
    mach_timebase_info_data_t _Base;

    bool Initialize() noexcept {
        return mach_timebase_info(&_Base)==KERN_SUCCESS;
    }

    bool _UU_Init = Initialize();
#endif
}

bool QueryPerformanceCounter(int64_t* lpPerformanceCount) noexcept {
    bool retval = true;
#if HAVE_MACH_ABSOLUTE_TIME
    *lpPerformanceCount = mach_absolute_time();
#elif HAVE_CLOCK_MONOTONIC
    struct timespec ts{};
    int result = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (result!=0) {
        retval = false;
    }
    else {
        *lpPerformanceCount=((int64_t)(ts.tv_sec)*(int64_t)(tccSecondsToNanoSeconds))+(int64_t)(ts.tv_nsec);
    }
#elif HAS_WIN32_CLOCK
    retVal = static_cast<bool>(QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(lpPerformanceCount)));
#else
#error "NO PROPPER CLOCK SUPPORT"
#endif
    return retval;
}

bool QueryPerformanceFrequency(int64_t* lpFrequency) noexcept {
    bool retval = true;
#if HAVE_MACH_ABSOLUTE_TIME
    if (_Base.denom==0) {
        retval = false;
    }
    else {
        *lpFrequency = ((int64_t) (tccSecondsToNanoSeconds)*(int64_t) (_Base.denom))/(int64_t) (_Base.numer);
    }
#elif HAVE_CLOCK_MONOTONIC
    *lpFrequency = tccSecondsToNanoSeconds;
#elif HAS_WIN32_CLOCK
    retVal = static_cast<bool>(QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(lpFrequency)));
#else
#error "NO PROPPER CLOCK SUPPORT"
#endif
    return retval;
}
