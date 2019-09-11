#pragma once

#include <new>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstddef>
#include <utility>
#include <type_traits>
#include <condition_variable>

#include "ThreadPool.h"
#include "Utilities/Threading/SpinWait.h"

// TODO: Reduce Allocation Calls
enum class FutureErrorCode : int {
    BrokenPromise = 0,
    FutureAlreadyRetrieved = 1,
    PromiseAlreadySatisfied = 2,
    NoState = 3
};

enum class ContinuationFlag : int {
    ExecOnCompletionInvocation = 0,
    ForceAsyncInvocation = 1,
    AsyncOnlyAfterDistantCompletion = 2
};

class AsyncContinuationContext {
public:
    void Reschedule(IExecTask* task) noexcept {
    }

    static AsyncContinuationContext* CaptureCurrent() noexcept {

    }
};

class AAsyncContinuationExecTask : public IExecTask {
public:
    void DoScheduleContinuation() noexcept {
        if (_Flag==ContinuationFlag::ExecOnCompletionInvocation) {
            this->Exec();
        }
        else {
            DoAsyncDispatchContinuation();
        }
    }

    void DoScheduleContinuationAsPromiseAlreadyFulfilled() noexcept {
        if (_Flag==ContinuationFlag::ForceAsyncInvocation) {
            DoAsyncDispatchContinuation();
        }
        else {
            this->Exec();
        }
    }

    void Setup(ContinuationFlag flag, AsyncContinuationContext* context) noexcept {
        SetFlag(flag);
        if (flag!=ContinuationFlag::ExecOnCompletionInvocation) {
            if (context) {
                SetContext(context);
            }
            else {
                Capture();
            }
        }
    }

    void SetFlag(ContinuationFlag flag) noexcept { _Flag = flag; }

    void SetContext(AsyncContinuationContext* context) noexcept { _Context = context; }

    void Capture() noexcept { SetContext(AsyncContinuationContext::CaptureCurrent()); }
private:
    ContinuationFlag _Flag;
    AsyncContinuationContext* _Context;

    void DoAsyncDispatchContinuation() noexcept { _Context->Reschedule(this); }
};

class FutureError : public std::logic_error {
public:
    explicit FutureError(FutureErrorCode ec);

    const auto& code() const noexcept { return _Code; }

    [[noreturn]] static void __throw(FutureErrorCode ec) { throw FutureError(ec); }
private:
    FutureErrorCode _Code;
};

namespace InterOp {
    class SharedAssociatedStateBase {
        // Frequent Use, kept apart with others
        mutable std::atomic<uintptr_t> _Lock{0};

        // state control
        enum LockFlags {
            SpinBit = 0b1,
            WriteBit = 0b10,
            ReadyBit = 0b100,
            PmrBitRev = 0b111
        };

        struct alignas(8) SyncPmrData {
            std::mutex Mutex{};
            std::condition_variable CV{};
        };

        auto GetPmrAddress() const noexcept {
            return reinterpret_cast<SyncPmrData*>(_Lock.load() & (~uintptr_t(PmrBitRev)));
        }

        auto EnablePmr() const {
            const auto newPmr = new(&__free_store) SyncPmrData();
            _Lock.fetch_or(reinterpret_cast<uintptr_t>(newPmr));
            return newPmr;
        }

        void NotifyIfPmrEnabled() const noexcept {
            if (const auto pmr = GetPmrAddress(); pmr) {
                std::lock_guard<std::mutex> lk(pmr->Mutex);
                pmr->CV.notify_all();
            }
        }

        bool TryAcquireWriteAccess() const noexcept {
            SpinWait spinner {};
            for (;;) {
                while (CheckWriteBit() && (!CheckReadyBit())) {
                    spinner.SpinOnce();
                }
                if (CheckReadyBit()) return false;
                auto _ = _Lock.load();
                if (_Lock.compare_exchange_strong(_, _ | WriteBit, std::memory_order_acquire)) return true;
            }
        }

        bool CheckSpinLock() const noexcept {
            return static_cast<bool>(_Lock.load(std::memory_order_relaxed) & SpinBit);
        }

        void AcquireSpinLock() const noexcept {
            size_t iter = 0;
            for (;;) {
                while (CheckSpinLock()) if (++iter<100) IDLE; else std::this_thread::yield();
                auto _ = _Lock.load();
                if (_Lock.compare_exchange_strong(_, _ | SpinBit, std::memory_order_acquire)) return;
            }
        }

        void ReleaseSpinLock() const noexcept { _Lock.fetch_and(~uintptr_t(SpinBit)); }

        auto PrepareWait() const {
            if (const auto addr = GetPmrAddress(); addr) return addr;
            AcquireSpinLock();
            const auto pmr = EnablePmr();
            ReleaseSpinLock();
            return pmr;
        }
    protected:
        void PrepareWrite() const {
            const auto success = TryAcquireWriteAccess();
            if (!success) FutureError::__throw(FutureErrorCode::PromiseAlreadySatisfied);
        }

        void CompleteWrite() noexcept {
            _Lock.fetch_or(ReadyBit);
            NotifyIfPmrEnabled();
            DoScheduleContinuation();
        }

        void PrepareWriteUnsafe() const noexcept { }

        void CompleteWriteUnsafe() noexcept {
            _Lock.fetch_or(ReadyBit | WriteBit);
            NotifyIfPmrEnabled();
            DoScheduleContinuation();
        }

        void CancelWrite() const noexcept { _Lock.fetch_and(~uintptr_t(WriteBit)); }
    public:
        void Wait() const {
            if (!IsReady()) {
                auto _ = PrepareWait();
                std::unique_lock<std::mutex> lk(_->Mutex);
                while (!IsReady())
                    _->CV.wait(lk);
            }
        }

        template <class Clock, class Duration>
        bool WaitUntil(const std::chrono::time_point<Clock, Duration>& absTime) const {
            if (!IsReady()) {
                auto _ = PrepareWait();
                std::unique_lock<std::mutex> lk(_->Mutex);
                while (!IsReady() && Clock::now()<absTime)
                    _->CV.wait_until(lk, absTime);
            }
            return IsReady();
        }

        template <class Rep, class Period>
        bool WaitFor(const std::chrono::duration<Rep, Period>& relTime) const {
            return WaitUntil(std::chrono::steady_clock::now()+relTime);
        }

        bool IsReady() const noexcept { return CheckNotExpiredReadyBit(); }
    private:
        bool CheckWriteBit() const noexcept { return static_cast<bool>(_Lock.load() & WriteBit); }

        bool CheckReadyBit() const noexcept { return static_cast<bool>(_Lock.load() & ReadyBit); }

        bool CheckNotExpiredReadyBit() const noexcept { return CheckReadyBit() && CheckWriteBit(); }

        void MakeExpire() const noexcept { _Lock.fetch_and(~uintptr_t(WriteBit)); }
    protected:
        void PrepareGet() const {
            Wait();
            AcquireSpinLock();
            if (!CheckNotExpiredReadyBit()) {
                ReleaseSpinLock();
                FutureError::__throw(FutureErrorCode::FutureAlreadyRetrieved);
            }
            MakeExpire();
            ReleaseSpinLock();
            if (_Exception)
                std::rethrow_exception(_Exception);
        }
    public:
        bool Satisfied() const noexcept { return CheckReadyBit(); }
        bool Valid() const noexcept { return CheckNotExpiredReadyBit() && _Continuation==nullptr; }
        // Continuable
    public:
        void SetContinuation(AAsyncContinuationExecTask* _task) noexcept {
            if (_Continuation.exchange(_task)==reinterpret_cast<AAsyncContinuationExecTask*>(uintptr_t(~0u))) {
                DoScheduleContinuationAsPromiseAlreadyFulfilled();
            }
        }
    private:
        std::atomic<AAsyncContinuationExecTask*> _Continuation{nullptr};

        AAsyncContinuationExecTask* GetContinuationExec() noexcept {
            return _Continuation.exchange(reinterpret_cast<AAsyncContinuationExecTask*>(uintptr_t(~0u)));
        }

        void DoScheduleContinuation() noexcept {
            if (const auto continuationExec = GetContinuationExec(); continuationExec) {
                continuationExec->DoScheduleContinuation();
            }
        }

        void DoScheduleContinuationAsPromiseAlreadyFulfilled() noexcept {
            if (const auto continuationExec = GetContinuationExec(); continuationExec) {
                continuationExec->DoScheduleContinuationAsPromiseAlreadyFulfilled();
            }
        }
    public:
        virtual ~SharedAssociatedStateBase() { if (auto _ = GetPmrAddress(); _) _->~SyncPmrData(); }

    private:
        // Data Store
        mutable std::atomic_int _RefCount{0};
        std::exception_ptr _Exception = nullptr;
    public:
        void SetException(const std::exception_ptr& p) {
            PrepareWrite();
            _Exception = p;
            CompleteWrite();
        }

        void SetExceptionUnsafe(const std::exception_ptr& p) noexcept {
            PrepareWriteUnsafe();
            _Exception = p;
            CompleteWriteUnsafe();
        }

        void Acquire() const noexcept { _RefCount.fetch_add(1); }

        void Release() const noexcept { if (_RefCount.fetch_sub(1)==1) delete this; }
    private:
        mutable std::aligned_storage_t<sizeof(SyncPmrData), alignof(SyncPmrData)> __free_store;
    };

    template <class T>
    class SharedAssociatedState final : public SharedAssociatedStateBase {
    public:
        ~SharedAssociatedState() override { reinterpret_cast<T*>(&_Value)->~T(); }

        void SetValue(T&& v) {
            PrepareWrite();
            if constexpr(std::is_nothrow_move_constructible_v<T>)
                new(&_Value) T(std::forward<T>(v));
            else
                try {
                    new(&_Value) T(std::forward<T>(v));
                }
                catch (...) {
                    CancelWrite();
                    throw;
                }
            CompleteWrite();
        }

        void SetValue(const T& v) {
            PrepareWrite();
            if constexpr(std::is_nothrow_copy_constructible_v<T>)
                new(&_Value) T(v);
            else
                try {
                    new(&_Value) T(v);
                }
                catch (...) {
                    CancelWrite();
                    throw;
                }
            CompleteWrite();
        }

        void SetValueUnsafe(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
            PrepareWriteUnsafe();
            new(&_Value) T(std::forward<T>(v));
            CompleteWriteUnsafe();
        }

        void SetValueUnsafe(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
            PrepareWriteUnsafe();
            new(&_Value) T(v);
            CompleteWriteUnsafe();
        }

        T Get() {
            PrepareGet();
            return std::move(*reinterpret_cast<T*>(&_Value));
        }
    private:
        std::aligned_storage_t<sizeof(T), alignof(T)> _Value;
    };

    template <class T>
    class SharedAssociatedState<T&> final : public SharedAssociatedStateBase {
    public:
        void SetValue(T& v) {
            PrepareWrite();
            _Value = std::addressof(v);
            CompleteWrite();
        }

        void SetValueUnsafe(T& v) noexcept {
            PrepareWriteUnsafe();
            _Value = std::addressof(v);
            CompleteWriteUnsafe();
        }

        T& Get() {
            PrepareGet();
            return *_Value;
        }
    private:
        T* _Value;
    };

    template <>
    class SharedAssociatedState<void> final : public SharedAssociatedStateBase {
    public:
        void SetValue() {
            PrepareWrite();
            CompleteWrite();
        }

        void SetValueUnsafe() noexcept {
            PrepareWriteUnsafe();
            CompleteWriteUnsafe();
        }

        void Get() const { PrepareGet(); }
    };

}

template <class T>
class Future;

template <class T>
class Promise;

template <class Callable, class ...Ts>
class DeferredCallable {
    template <std::size_t... I>
    auto Apply(std::index_sequence<I...>) { return _Function(std::move(std::get<I>(_Arguments))...); }
protected:
    using ReturnType = std::invoke_result_t<std::decay_t<Callable>, std::decay_t<Ts>...>;
public:
    explicit DeferredCallable(Callable&& call, Ts&& ... args)
            :_Function(std::forward<Callable>(call)), _Arguments(std::forward<Ts>(args)...) { }
    auto Invoke() { return Apply(std::make_index_sequence<std::tuple_size<decltype(_Arguments)>::value>()); }
private:
    Callable _Function;
    std::tuple<Ts...> _Arguments;
};

template <class Callable>
class DeferredCallable<Callable> {
protected:
    using ReturnType = std::invoke_result_t<std::decay_t<Callable>>;
public:
    explicit DeferredCallable(Callable&& call)
            :_Function(std::forward<Callable>(call)) { }
    auto Invoke() { return _Function(); }
private:
    Callable _Function;
};

template <class Callable, class ...Ts>
class DeferredProcedureCallTask : public AAsyncContinuationExecTask, DeferredCallable<Callable, Ts...> {
public:
    using ReturnType = typename DeferredCallable<Callable, Ts...>::ReturnType;

    explicit DeferredProcedureCallTask(Callable&& call, Ts&& ... args)
            :DeferredCallable<Callable, Ts...>(std::forward<Callable>(call), std::forward<Ts>(args)...) { }

    void Exec() noexcept override {
        try {
            if constexpr(std::is_same_v<ReturnType, void>) {
                DeferredCallable<Callable, Ts...>::Invoke();
                _Promise.SetValueUnsafe();
            }
            else
                _Promise.SetValueUnsafe(DeferredCallable<Callable, Ts...>::Invoke());
        }
        catch (...) {
            _Promise.SetExceptionUnsafe(std::current_exception());
        }
        delete this;
    }

    auto GetFuture() { return _Promise.GetFuture(); }
private:
    Promise<ReturnType> _Promise{};
};

namespace InterOp {
    template <class T>
    class FutureBase {
    protected:
        explicit FutureBase(SharedAssociatedState<T>* _) noexcept
                :_State(_) { _State->Acquire(); }
    public:
        FutureBase() = default;

        FutureBase(FutureBase&& other) noexcept
                :_State(other._State) { other._State = nullptr; }

        FutureBase& operator=(FutureBase&& other) noexcept {
            if (this!=std::addressof(other)) {
                _State = other._State;
                other._State = nullptr;
            }
            return *this;
        }
        FutureBase(const FutureBase&) = delete;

        FutureBase& operator=(const FutureBase&) = delete;

        ~FutureBase() { ReleaseState(); }

        bool Valid() const noexcept { return _State ? _State->Valid() : false; }

        bool IsReady() const noexcept { return _State->IsReady(); }

        void Wait() const { _State->Wait(); }

        template <class _Clock, class _Duration>
        bool WaitUntil(const std::chrono::time_point<_Clock, _Duration>& time) const {
            return _State->WaitUntil(time);
        }

        template <class _Rep, class _Period>
        bool WaitFor(const std::chrono::duration<_Rep, _Period>& relTime) const {
            return _State->WaitUntil(relTime);
        }

        template <class Func>
        auto Then(Func fn, ContinuationFlag flag = ContinuationFlag::ExecOnCompletionInvocation,
                AsyncContinuationContext* context = nullptr) {
            auto task = std::make_unique<DeferredProcedureCallTask<std::decay_t<Func>, Future<T>>>(
                    std::forward<std::decay_t<Func>>(std::move(fn)),
                    Future(nullptr, _State)
            );
            task->Setup(flag, context);
            auto future = task->GetFuture();
            _State->SetContinuation(task.release(), flag);
            _State = nullptr;
            return future;
        }
    protected:
        void ReleaseState() noexcept {
            if (_State) {
                _State->Release();
                _State = nullptr;
            }
        }

        struct ReleaseRAII {
            explicit ReleaseRAII(FutureBase* _) noexcept
                    :_This(_) { }
            ReleaseRAII(ReleaseRAII&&) = delete;
            ReleaseRAII& operator=(ReleaseRAII&&) = delete;
            ~ReleaseRAII() noexcept { _This->ReleaseState(); }
            FutureBase* _This;
        };

        auto CreateRAII() noexcept { return ReleaseRAII(this); }

        InterOp::SharedAssociatedState<T>* _State = nullptr;
    };

    template <class T>
    class PromiseBase {
    public:
        PromiseBase() = default;

        PromiseBase(PromiseBase&& other) noexcept
                :_State(other._State) { other._State = nullptr; }

        PromiseBase& operator=(PromiseBase&& other) noexcept {
            if (this!=std::addressof(other)) {
                _State = other._State;
                other._State = nullptr;
            }
            return *this;
        }

        PromiseBase(const PromiseBase&) = delete;

        PromiseBase& operator=(const PromiseBase&) = delete;

        ~PromiseBase() noexcept {
            if (_State) {
                if (!_State->Satisfied())
                    SetExceptionUnsafe(std::make_exception_ptr(FutureError(FutureErrorCode::BrokenPromise)));
                _State->Release();
            }
        }

        Future<T> GetFuture() { return Future<T>(_State); }

        void SetException(std::exception_ptr p) { _State->SetException(p); };

        void SetExceptionUnsafe(std::exception_ptr p) noexcept { _State->SetExceptionUnsafe(p); }
    protected:
        SharedAssociatedState<T>& GetState() {
            if (_State) return *_State;
            FutureError::__throw(FutureErrorCode::NoState);
        }
        SharedAssociatedState<T>* _State = MakeState();
    private:
        static SharedAssociatedState<T>* MakeState() {
            auto _ = new SharedAssociatedState<T>;
            _->Acquire();
            return _;
        }
    };
}

template <class T>
class Future : public InterOp::FutureBase<T> {
    friend class InterOp::PromiseBase<T>;
    friend class InterOp::FutureBase<T>;
    explicit Future(InterOp::SharedAssociatedState<T>* _) noexcept
            :InterOp::FutureBase<T>(_) { }

    Future(std::nullptr_t,
            InterOp::SharedAssociatedState<T>* _) noexcept { InterOp::FutureBase<T>::template _State = _; }
public:
    Future() noexcept = default;
    auto Get() {
        auto _ = InterOp::FutureBase<T>::template CreateRAII();
        return InterOp::FutureBase<T>::template _State->Get();
    }
};

template <class T>
class Future<T&> : public InterOp::FutureBase<T&> {
    friend class InterOp::PromiseBase<T&>;
    friend class InterOp::FutureBase<T&>;
    explicit Future(InterOp::SharedAssociatedState<T&>* _) noexcept
            :InterOp::FutureBase<T&>(_) { }

    Future(std::nullptr_t,
            InterOp::SharedAssociatedState<T&>* _) noexcept { InterOp::FutureBase<T&>::template _State = _; }
public:
    Future() noexcept = default;
    auto& Get() {
        auto _ = InterOp::FutureBase<T&>::template CreateRAII();
        return InterOp::FutureBase<T&>::template _State->Get();
    }
};

template <>
class Future<void> : public InterOp::FutureBase<void> {
    friend class InterOp::PromiseBase<void>;
    friend class InterOp::FutureBase<void>;
    explicit Future(InterOp::SharedAssociatedState<void>* _) noexcept
            :FutureBase<void>(_) { }

    Future(std::nullptr_t, InterOp::SharedAssociatedState<void>* _) noexcept { _State = _; }
public:
    Future() noexcept = default;
    void Get() {
        auto _ = CreateRAII();
        _State->Get();
    }
};

template <class T>
class Promise : public InterOp::PromiseBase<T> {
public:
    void SetValue(T&& v) { InterOp::PromiseBase<T>::template GetState().SetValue(std::move(v)); }

    void SetValue(const T& v) { InterOp::PromiseBase<T>::template GetState().SetValue(v); }

    void SetValueUnsafe(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>) {
        InterOp::PromiseBase<T>::template GetState().SetValueUnsafe(std::move(v));
    }

    void SetValueUnsafe(const T& v) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        InterOp::PromiseBase<T>::template GetState().SetValueUnsafe(v);
    }
};

template <class T>
class Promise<T&> : public InterOp::PromiseBase<T&> {
public:
    void SetValue(T& v) { InterOp::PromiseBase<T&>::template GetState().SetValue(v); }

    void SetValueUnsafe(T& v) noexcept {
        InterOp::PromiseBase<T&>::template GetState().SetValueUnsafe(v);
    }
};

template <>
class Promise<void> : public InterOp::PromiseBase<void> {
public:
    void SetValue() { GetState().SetValue(); }

    void SetValueUnsafe() noexcept { GetState().SetValueUnsafe(); }
};


namespace InterOp {
    void AsyncResumePrevious() noexcept;

    IExecTask* AsyncGetCurrent() noexcept;

    void AsyncCall(IExecTask* inner) noexcept;
}

template <template <class> class Cont, class U>
U Await(Cont<U> cont) {
    if constexpr (std::is_same_v<U, void>) {
        auto fu = cont.then([task = InterOp::AsyncGetCurrent()](auto&& lst) {
            ThreadPool::Enqueue(std::unique_ptr<IExecTask>(task));
            lst.get();
        });
        InterOp::AsyncResumePrevious();
        fu.get();
    }
    else {
        auto fu = cont.then([task = InterOp::AsyncGetCurrent()](auto&& lst) {
            ThreadPool::Enqueue(std::unique_ptr<IExecTask>(task));
            return lst.get();
        });
        InterOp::AsyncResumePrevious();
        return fu.get();
    }
}

template <class Func, class ...Ts>
auto Async(Func __fn, Ts&& ... args) {
    auto inner_task = new DeferredProcedureCallTask (
            std::forward<std::decay_t<Func>>(std::move(__fn)),
            std::forward<Ts>(args)...
    );
    auto future = inner_task->GetFuture();
    AsyncCall(inner_task);
    return future;
}
