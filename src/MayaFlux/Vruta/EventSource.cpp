#include "EventSource.hpp"

#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"

namespace MayaFlux::Vruta {

void EventSource::dispatch(const void* event)
{
    if (m_waiters.empty())
        return;

    auto waiters = m_waiters;
    for (auto* w : waiters)
        w->try_resume(event);
}

void EventSource::register_waiter(Kriya::EventAwaiter* awaiter)
{
    m_waiters.push_back(awaiter);
}

void EventSource::unregister_waiter(Kriya::EventAwaiter* awaiter)
{
    auto it = std::ranges::find(m_waiters, awaiter);
    if (it != m_waiters.end())
        m_waiters.erase(it);
}

} // namespace MayaFlux::Vruta
