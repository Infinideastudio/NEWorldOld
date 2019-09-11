#include "Promise.h"
#include <boost/context/fiber.hpp>

namespace {
    const char* GetErrorString(FutureErrorCode code) noexcept {
        switch (code) {
        case FutureErrorCode::BrokenPromise: return "FutureError:broken promise";
        case FutureErrorCode::FutureAlreadyRetrieved: return "FutureError:future already retrieved";
        case FutureErrorCode::PromiseAlreadySatisfied: return "FutureError:promise already satisfied";
        case FutureErrorCode::NoState: return "FutureError:no state";
        }
    }

    class AsyncExecutor : public IExecTask {
    public:
        explicit AsyncExecutor(IExecTask* inner)
                :
                _Current(boost::context::fiber([this, inner](boost::context::fiber&& sink) mutable noexcept {
                    _Sink = std::move(sink);
                    inner->Exec();
                    return std::move(_Sink);
                })) { }
        void Exec() noexcept override;
        void ResumeSink() { _Sink = std::move(_Sink).resume(); }
    private:
        boost::context::fiber _Current, _Sink;
    };

    thread_local AsyncExecutor* _CurrentExecutor = nullptr;

    void AsyncExecutor::Exec() noexcept {
        _CurrentExecutor = this;
        _Current = std::move(_Current).resume();
        if (!static_cast<bool>(_Current))
            delete this;
    }
}

FutureError::FutureError(FutureErrorCode ec):logic_error(GetErrorString(ec)), _Code(ec) { }

namespace InterOp {
    void AsyncResumePrevious() noexcept { _CurrentExecutor->ResumeSink(); }

    IExecTask* AsyncGetCurrent() noexcept { return _CurrentExecutor; }

    void AsyncCall(IExecTask* inner) noexcept {
        ThreadPool::Enqueue(std::make_unique<AsyncExecutor>(inner));
    }
}
