#include "Promise.hpp"

namespace MayaFlux::Core::Scheduler {

// SoundRoutine promise_type::get_return_object()
// {
//     return SoundRoutine(std::coroutine_handle<promise_type>::from_promise(*this));
// }

void GetPromise::await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
    promise_ptr = &h.promise();
}

promise_handle& GetPromise::await_resume() const noexcept
{
    return *promise_ptr;
}

void await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
}

}
