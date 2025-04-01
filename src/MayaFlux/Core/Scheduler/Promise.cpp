#include "Promise.hpp"

namespace MayaFlux::Core::Scheduler {

// SoundRoutine promise_type::get_return_object()
// {
//     return SoundRoutine(std::coroutine_handle<promise_type>::from_promise(*this));
// }

// Constructor implementation
GetPromise::GetPromise(promise_handle& p)
    : promise(p)
{
}

// Implementation of GetPromise methods
bool GetPromise::await_ready() const noexcept { return false; }

void GetPromise::await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
    promise = h.promise();
}

promise_handle& GetPromise::await_resume() const noexcept
{
    return promise;
}

}
