#pragma once

#include "SpinWait.h"

class SpinLock {
public:
    void Enter() noexcept { while (__lock.exchange(true, std::memory_order_acquire)) waitUntilLockIsFree(); }

    void Leave() noexcept { __lock.store(false, std::memory_order_release); }
private:
    void waitUntilLockIsFree() const noexcept {
        SpinWait spinner{};
        while (__lock.load(std::memory_order_relaxed)) {
            spinner.SpinOnce();
        }
    }
    alignas(64) std::atomic_bool __lock = {false};
};
