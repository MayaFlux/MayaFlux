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

    if (auto ev = m_source.pop_event(m_filter)) {
        m_result = *ev;
        m_source.unregister_waiter(this);
        m_is_suspended = false;
        m_handle.resume();
    }
}

bool WindowEventAwaiter::filter_matches(const void* event) const
{
    const auto* ev = static_cast<const Core::WindowEvent*>(event);

    if (m_filter.event_type && ev->type != *m_filter.event_type)
        return false;

    if (m_filter.key_code) {
        const auto* kd = std::get_if<Core::WindowEvent::KeyData>(&ev->data);
        if (!kd || kd->key != static_cast<int16_t>(*m_filter.key_code))
            return false;
    }

    if (m_filter.button) {
        const auto* bd = std::get_if<Core::WindowEvent::MouseButtonData>(&ev->data);
        if (!bd || bd->button != static_cast<int>(*m_filter.button))
            return false;
    }

    return true;
}

} // namespace MayaFlux::Kriya
