#include "Awaiters.hpp"

namespace MayaFlux::Kriya {

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

void SampleDelay::await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
    if constexpr (std::is_same_v<promise_handle, Vruta::audio_promise>
        || std::is_same_v<promise_handle, Vruta::complex_promise>) {
        if constexpr (requires { h.promise().next_sample; }) {
            h.promise().next_sample += samples_to_wait;
        }
    } else {
        if constexpr (requires { h.promise().domain_mismatch_error("", ""); }) {
            h.promise().domain_mismatch_error("SampleDelay",
                "This awaitable is not compatible with the current coroutine promise type.");
        }
    }
}

void FrameDelay::await_suspend(std::coroutine_handle<Vruta::graphics_promise> h) noexcept
{
    // TODO: Implement when graphics_promise is ready
    if constexpr (std::is_same_v<promise_handle, Vruta::graphics_promise>
        || std::is_same_v<promise_handle, Vruta::complex_promise>) {
        if constexpr (requires { h.promise().next_frame; }) {
            h.promise().next_frame += frames_to_wait;
        }
    } else {
        if constexpr (requires { h.promise().domain_mismatch_error("", ""); }) {
            h.promise().domain_mismatch_error("FrameDelay",
                "This awaitable is not compatible with the current coroutine promise type.");
        }
    }
}

void MultiRateDelay::await_suspend(std::coroutine_handle<Vruta::complex_promise> h) noexcept
{
    if constexpr (requires { h.promise().next_sample; }) {
        h.promise().next_sample += samples_to_wait;
    }
    if constexpr (requires { h.promise().next_frame; }) {
        h.promise().next_frame += frames_to_wait;
    }
}
}
