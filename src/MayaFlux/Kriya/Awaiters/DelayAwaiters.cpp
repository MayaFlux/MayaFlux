#include "DelayAwaiters.hpp"

namespace MayaFlux::Kriya {

void SampleDelay::await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
    if constexpr (std::is_same_v<promise_handle, Vruta::audio_promise>
        || std::is_same_v<promise_handle, Vruta::complex_promise>) {
        if constexpr (requires { h.promise().next_sample; }) {
            h.promise().next_sample += samples_to_wait;
            h.promise().active_delay_context = Vruta::DelayContext::SAMPLE_BASED;
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
    if constexpr (std::is_same_v<promise_handle, Vruta::graphics_promise>
        || std::is_same_v<promise_handle, Vruta::complex_promise>) {
        if constexpr (requires { h.promise().next_frame; }) {
            h.promise().next_frame += frames_to_wait;
            h.promise().active_delay_context = Vruta::DelayContext::FRAME_BASED;
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
