#include "ThreadPool.h"
#include "Utilities/Threading/SpinLock.h"
#include "Utilities/Threading/Semaphore.h"
#include <deque>

namespace {
    // Brute-Force Implementation!
    // TODO: Improve It;
    class Queue {
    public:
        Queue* _Next;

        void Push(IExecTask* task) {
            _Spin.Enter();
            _Exec.push_back(task);
            _Spin.Leave();
        }

        IExecTask* Pop() {
            IExecTask* ret = nullptr;
            _Spin.Enter();
            if (!_Exec.empty()) {
                ret = _Exec.front();
                _Exec.pop_front();
            }
            _Spin.Leave();
            return ret;
        }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-container-size-empty"
        IExecTask* TryDequeue() {
            for (Queue* current = this; current!=this; current = current->_Next) {
                if (current->_Exec.size()) { // Dangerous Operation!
                    if (const auto res = current->Pop(); res) {
                        return res;
                    }
                }
            }
            return nullptr;
        }
#pragma clang diagnostic pop
    private:
        SpinLock _Spin;
        std::deque<IExecTask*> _Exec;
    };

    class QueueGroup {
    public:
        QueueGroup() {
            _First = &_Global;
            _Global._Next = _First;
        }

        void Add(Queue* queue) noexcept {
            _Lock.Enter();
            queue->_Next = _First->_Next;
            _First->_Next = queue;
            _Lock.Leave();
        }

        static auto& Instance() {
            static QueueGroup instance;
            return instance;
        }

        Queue& Global() noexcept { return _Global; }
    private:
        SpinLock _Lock;
        Queue* _First;
        Queue _Global;
    };

    auto& GetCurrentQueue() {
        static thread_local Queue* queue;
        return queue;
    }

    std::vector<std::thread> _Workers;

    void SetupQueue() {
        auto& queue = GetCurrentQueue();
        queue = new Queue();
        QueueGroup::Instance().Add(queue);
    }

    std::atomic_bool _Running {false}, _Panicking{false};

    struct Panic : std::exception {};

    [[noreturn]] void WorkerPanic() { throw Panic{}; }

    bool Running() noexcept { return _Running; }

    IExecTask* FetchWork(Queue* queue) noexcept {
        if (_Panicking) {
            WorkerPanic();
        }
        if (auto exec = queue->TryDequeue(); exec) {
            return exec;
        }
        else {
            SpinWait spinner{};
            for (int i = 0; i < SpinWait::SpinCountforSpinBeforeWait; ++i) {
                spinner.SpinOnce();
                if (exec = queue->TryDequeue(); exec) {
                    return exec;
                }
            }
            return nullptr;
        }
    }

    void DoWorks() {
        const auto queue = GetCurrentQueue();
        for (;;) {
            if (auto exec = FetchWork(queue); exec) {
                exec->Exec();
            }
            else {
                return;
            }
        }
    }

    std::atomic_int _Parked;
    Semaphore _ParkingLot {};

    void Rest() {
        _Parked.fetch_add(1);
        _ParkingLot.Wait();
        if (_Panicking) {
            WorkerPanic();
        }
    }

    void PanicIfNotAlready() {
        if (!_Panicking.exchange(true)) {
            ThreadPool::Panic();
        }
    }

    thread_local int _InstanceInvokeId;
    std::atomic_int _MaxId = 0;

    void SetInstanceInvokeId() {
        _InstanceInvokeId = _MaxId.fetch_add(1) - 1;
    }

    void Worker() {
        SetInstanceInvokeId();
        SetupQueue();
        try {
            while (Running()) {
                DoWorks();
                Rest();
            }
        }
        catch(...) {
            PanicIfNotAlready();
        }
    }

    void WakeAll() noexcept {
        auto c = _Parked.exchange(0);
        for (int i = 0; i < c; ++i) {
            _ParkingLot.Signal();
        }
    }

    void WakeOne() noexcept {
        for (;;) {
            if (auto c = _Parked.load(); c) {
                if (_Parked.compare_exchange_strong(c, c - 1)) {
                    _ParkingLot.Signal();
                    return;
                }
            }
            else {
                return;
            }
        }
    }

    struct RaiiStop {
        ~RaiiStop() {
            ThreadPool::Stop();
        }
    };

    RaiiStop Start(int threadCount) {
        for (int i = 0; i < threadCount; i++) {
            _Workers.emplace_back(Worker);
        }
        return RaiiStop();
    }

    RaiiStop _UU_Init = Start(std::thread::hardware_concurrency() - 1);
}

bool ThreadPool::LocalEnqueue(std::unique_ptr<IExecTask>& task) {
    if (const auto queue = GetCurrentQueue(); queue) {
        queue->Push(task.release());
    }
    return false;
}

void ThreadPool::Enqueue(std::unique_ptr<IExecTask> task) {
    if (!LocalEnqueue(task)) {
        QueueGroup::Instance().Global().Push(task.release());
    }
    WakeOne();
}

void ThreadPool::Spawn(std::unique_ptr<AInstancedExecTask> task) {
    auto zero = &QueueGroup::Instance().Global();
    auto taskRaw = task.release();
    for (Queue* current = zero; current!=zero; current = current->_Next) {
        current->Push(taskRaw);
    }
    WakeAll();
}

void ThreadPool::Stop() {
    _Running = false;
    WakeAll();
    for (auto&& x : _Workers) {
        if (x.joinable()) {
            x.join();
        }
    }
}

void ThreadPool::Panic() noexcept {
    _Panicking = true;
    Stop();
}

void AInstancedExecTask::Exec() noexcept {
    Exec(_InstanceInvokeId);
}
