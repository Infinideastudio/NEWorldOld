#pragma once

#include <thread>
#include <memory>
#include <vector>

class IExecTask {
public:
    virtual void Exec() noexcept = 0;
    virtual ~IExecTask() noexcept = default;
};

class AInstancedExecTask : public IExecTask{
public:
    void Exec() noexcept override;
    virtual void Exec(uint32_t instance) noexcept = 0;
};

class ThreadPool {
public:
    static bool LocalEnqueue(std::unique_ptr<IExecTask>& task);
    static void Enqueue(std::unique_ptr<IExecTask> task);
    static void Spawn(std::unique_ptr<AInstancedExecTask> task);
    static void Stop();
    static void Panic() noexcept;
};