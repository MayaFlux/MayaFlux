#include "Awaiters.hpp"

namespace MayaFlux::Tasks {

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
