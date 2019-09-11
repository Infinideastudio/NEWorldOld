#include "SpinWait.h"
#include "Timings.h"
#include <cmath>

bool SpinWait::IsSingleProcessor = std::thread::hardware_concurrency()==1;
int SpinWait::SpinCountforSpinBeforeWait = IsSingleProcessor ? 1 : 35;
int SpinWait::OptimalMaxSpinWaitsPerSpinIteration = 7;

namespace {
    constexpr unsigned int MinNsPerNormalizedYield = 37; // measured typically 37-46 on post-Skylake
    constexpr unsigned int NsPerOptimalMaxSpinIterationDuration = 272; // approx. 900 cycles, measured 281 on pre-Skylake, 263 on post-Skylake

    bool InitializeYieldProcessorNormalized() noexcept {
        // Intel pre-Skylake processor: measured typically 14-17 cycles per yield
        // Intel post-Skylake processor: measured typically 125-150 cycles per yield
        const int MeasureDurationMs = 10;
        const int NsPerSecond = 1000*1000*1000;

        int64_t ticksPerSecond;
        if (!QueryPerformanceFrequency(&ticksPerSecond) || ticksPerSecond<1000/MeasureDurationMs) {
            // High precision clock not available or clock resolution is too low, resort to defaults
            return true;
        }

        // Measure the nanosecond delay per yield
        int64_t measureDurationTicks = ticksPerSecond/(1000/MeasureDurationMs);
        unsigned int yieldCount = 0;
        int64_t startTicks;
        QueryPerformanceCounter(&startTicks);
        int64_t elapsedTicks;
        do {
            // On some systems, querying the high performance counter has relatively significant overhead. Do enough yields to mask
            // the timing overhead. Assuming one yield has a delay of MinNsPerNormalizedYield, 1000 yields would have a delay in the
            // low microsecond range.
            for (int i = 0; i<1000; ++i) {
                IDLE;
            }
            yieldCount += 1000;

            int64_t nowTicks;
            QueryPerformanceCounter(&nowTicks);
            elapsedTicks = nowTicks-startTicks;
        }
        while (elapsedTicks<measureDurationTicks);
        double nsPerYield = (double) elapsedTicks*NsPerSecond/((double) yieldCount*ticksPerSecond);
        if (nsPerYield<1) {
            nsPerYield = 1;
        }

        // Calculate the number of yields required to span the duration of a normalized yield. Since nsPerYield is at least 1, this
        // value is naturally limited to MinNsPerNormalizedYield.
        int yieldsPerNormalizedYield = lround(MinNsPerNormalizedYield/nsPerYield);
        if (yieldsPerNormalizedYield<1) {
            yieldsPerNormalizedYield = 1;
        }

        // Calculate the maximum number of yields that would be optimal for a late spin iteration. Typically, we would not want to
        // spend excessive amounts of time (thousands of cycles) doing only YieldProcessor, as SwitchToThread/Sleep would do a
        // better job of allowing other work to run.
        int optimalMaxNormalizedYieldsPerSpinIteration = lround(
                NsPerOptimalMaxSpinIterationDuration/(yieldsPerNormalizedYield*nsPerYield));
        if (optimalMaxNormalizedYieldsPerSpinIteration<1) {
            optimalMaxNormalizedYieldsPerSpinIteration = 1;
        }

        SpinWait::OptimalMaxSpinWaitsPerSpinIteration = optimalMaxNormalizedYieldsPerSpinIteration;
        return true;
    }

    bool _UU_Init = InitializeYieldProcessorNormalized();
}
