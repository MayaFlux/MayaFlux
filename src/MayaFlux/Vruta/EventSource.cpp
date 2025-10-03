#include "EventSource.hpp"

#include <algorithm>

#include "MayaFlux/Kriya/Awaiters.hpp"

namespace MayaFlux::Vruta {

void EventSource::signal(Core::WindowEvent event)
{
    m_pending_events.push(event);

    auto waiters = m_waiters;
    for (auto* awaiter : waiters) {
        awaiter->try_resume();
    }
}

Kriya::EventAwaiter EventSource::next_event()
{
    return Kriya::EventAwaiter { *this, std::nullopt };
}

Kriya::EventAwaiter EventSource::await_event(Core::WindowEventType type)
{
    return Kriya::EventAwaiter { *this, type };
}

std::optional<Core::WindowEvent> EventSource::pop_event(
    std::optional<Core::WindowEventType> filter)
{
    if (m_pending_events.empty()) {
        return std::nullopt;
    }

    if (!filter) {
        auto event = m_pending_events.front();
        m_pending_events.pop();
        return event;
    }

    // TODO: Filter for specific type (need to search queue)
    auto event = m_pending_events.front();
    if (event.type == *filter) {
        m_pending_events.pop();
        return event;
    }

    return std::nullopt;
}

void EventSource::register_waiter(Kriya::EventAwaiter* awaiter)
{
    m_waiters.push_back(awaiter);
}

void EventSource::unregister_waiter(Kriya::EventAwaiter* awaiter)
{
    auto it = std::ranges::find(m_waiters, awaiter);
    if (it != m_waiters.end()) {
        m_waiters.erase(it);
    }
}

} // namespace MayaFlux::Vruta
