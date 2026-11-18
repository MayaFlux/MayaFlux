#include "EventAwaiter.hpp"

namespace MayaFlux::Kriya {

WindowEventAwaiter::~WindowEventAwaiter()
{
    if (m_is_suspended)
        m_source.unregister_waiter(this);
}

bool WindowEventAwaiter::await_ready()
{
    if (auto ev = m_source.pop_event(m_filter)) {
        m_result = *ev;
        return true;
    }
    return false;
}

void WindowEventAwaiter::await_suspend(std::coroutine_handle<> handle)
{
    m_handle = handle;
    m_is_suspended = true;
    m_source.register_waiter(this);
}

Core::WindowEvent WindowEventAwaiter::await_resume()
{
    m_is_suspended = false;
    return m_result;
}

void WindowEventAwaiter::try_resume(const void* event)
{
    if (!filter_matches(event))
        return;

    m_result = *static_cast<const Core::WindowEvent*>(event);
    ++m_source.m_resumed;
    m_source.unregister_waiter(this);
    m_is_suspended = false;
    m_handle.resume();
}

bool WindowEventAwaiter::filter_matches(const void* event) const
{
    return m_filter.matches(*static_cast<const Core::WindowEvent*>(event));
}

} // namespace MayaFlux::Kriya
