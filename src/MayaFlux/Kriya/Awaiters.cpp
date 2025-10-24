#include "Awaiters.hpp"
#include "MayaFlux/Vruta/EventSource.hpp"

namespace MayaFlux::Kriya {

void GetPromise::await_suspend(std::coroutine_handle<promise_handle> h) noexcept
{
    promise_ptr = &h.promise();
}

promise_handle& GetPromise::await_resume() const noexcept
{
    return *promise_ptr;
}

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

EventAwaiter::~EventAwaiter()
{
    if (m_is_suspended) {
        m_source.unregister_waiter(this);
    }
}

bool EventAwaiter::await_ready()
{
    if (auto event = m_source.pop_event(m_filter)) {
        m_result = *event;
        return true;
    }
    return false;
}

void EventAwaiter::await_suspend(std::coroutine_handle<> handle)
{
    m_handle = handle;
    m_is_suspended = true;
    m_source.register_waiter(this);
}

Core::WindowEvent EventAwaiter::await_resume()
{
    m_is_suspended = false;
    return m_result;
}

void EventAwaiter::try_resume()
{
    if (auto event = m_source.pop_event(m_filter)) {
        m_result = *event;
        m_source.unregister_waiter(this);
        m_is_suspended = false;
        m_handle.resume();
    }
}

}
