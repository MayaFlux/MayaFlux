#include "EventAwaiter.hpp"

namespace MayaFlux::Kriya {

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
