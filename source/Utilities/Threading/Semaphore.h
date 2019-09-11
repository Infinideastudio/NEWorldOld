#pragma once

#if __has_include(<mach/semaphore.h>)
#include <mach/semaphore.h>
#include <mach/mach_init.h>
#include <mach/task.h>

class Semaphore {
public:
    explicit Semaphore() noexcept
            :_Handle(New()) { }

    ~Semaphore() { Release(_Handle); }

    void Wait() noexcept {
        while (semaphore_wait(_Handle)!=KERN_SUCCESS) { }
    }

    void Signal() noexcept { semaphore_signal(_Handle); }
private:
    static semaphore_t New() noexcept {
        semaphore_t ret;
        semaphore_create(mach_task_self(), &ret, SYNC_POLICY_FIFO, 0);
        return ret;
    }

    static void Release(semaphore_t sem) noexcept {
        semaphore_destroy(mach_task_self(), sem);
    }

    semaphore_t _Handle;
};

#elif __has_include(<Windows.h>)
#define _WIN32_LEAN_AND_MEAN
#include <Windows.h>
class Semaphore {
public:
    Semaphore() noexcept
            :_Handle(CreateSemaphore(nullptr, 0, MAXLONG, nullptr)) { }

    ~Semaphore() noexcept {
        CloseHandle(_Handle);
    }

    void Wait() noexcept {
        WaitForSingleObject(_Handle, INFINITE);
    }

    void Signal() noexcept {
        int __last;
        ReleaseSemaphore(_Handle, 1, &__last);
    }
private:
    HANDLE _Handle;
};

#elif __has_include(<semaphore.h>)
#include <semaphore.h>
class Semaphore {
public:
    Semaphore() noexcept {
        sem_init(&_Semaphore, 0, 0);
    }

    ~Semaphore() noexcept {
        sem_destroy(&_Semaphore);
    }

    void Wait() noexcept {
        sem_wait(&_Semaphore);
    }

    void Signal() noexcept {
        sem_post(&_Semaphore);
    }
private:
    sem_t _Semaphore;
};
#endif