#pragma once

#include <atomic>
#include <thread>
#include <limits>

#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#define IDLE _mm_pause()
#elif __has_include(<intrin.h>)
#include <intrin.h>
#define IDLE _mm_pause()
#else
#define IDLE asm("nop")
#endif

struct SpinWait {
    static constexpr int YieldThreshold = 10; // When to switch over to a true yield.
    static constexpr int Sleep0EveryHowManyYields = 5; // After how many yields should we Sleep(0)?
    static constexpr int DefaultSleep1Threshold = 20; // After how many yields should we Sleep(1) frequently?
    static bool IsSingleProcessor;
    static int OptimalMaxSpinWaitsPerSpinIteration;
public:
    static int SpinCountforSpinBeforeWait;

    int Count() const noexcept { return _Count; }

    bool NextSpinWillYield() const noexcept { return _Count>=YieldThreshold || IsSingleProcessor; }

    void SpinOnce() noexcept {
        SpinOnceCore(DefaultSleep1Threshold);
    }

    void SpinOnce(int sleep1Threshold) noexcept {
        if (sleep1Threshold>=0 && sleep1Threshold<YieldThreshold) {
            sleep1Threshold = YieldThreshold;
        }
        SpinOnceCore(sleep1Threshold);
    }

    void Reset() noexcept {
        _Count = 0;
    }

private:
    unsigned int _Count;

    void SpinOnceCore(int sleep1Threshold) noexcept {
        if ((_Count>=YieldThreshold
                && ((_Count>=sleep1Threshold && sleep1Threshold>=0) || (_Count-YieldThreshold)%2==0))
                || IsSingleProcessor) {
            if (_Count>=sleep1Threshold && sleep1Threshold>=0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            else {
                int yieldsSoFar = _Count>=YieldThreshold ? (_Count-YieldThreshold)/2 : _Count;
                if ((yieldsSoFar%Sleep0EveryHowManyYields)==(Sleep0EveryHowManyYields-1)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(0));
                }
                else {
                    std::this_thread::yield();
                }
            }
        }
        else {
            int n = OptimalMaxSpinWaitsPerSpinIteration;
            if (_Count<=30 && (1u << _Count)<n) {
                n = 1u << _Count;
            }
            Spin(n);
        }

        // Finally, increment our spin counter.
        _Count = (_Count==std::numeric_limits<int>::max() ? YieldThreshold : _Count + 1);
    }

    void Spin(int iterations) noexcept {
        for (int i = 0; i<iterations; ++i) {
            IDLE;
        }
    }
};