#include "Event.hpp"

namespace Lila {

Event::Event(EventType t, EventData d)
    : type(t)
    , data(std::move(d))
    , timestamp(std::chrono::system_clock::now())
{
}

void EventBus::subscribe(EventType type, const std::shared_ptr<Subscription>& subscriber)
{
    std::lock_guard lock(m_mutex);
    m_subscribers[type].emplace_back(subscriber);
}

void EventBus::publish(const Event& event)
{
    std::vector<std::shared_ptr<Subscription>> active_subscribers;

    {
        std::lock_guard lock(m_mutex);
        auto it = m_subscribers.find(event.type);
        if (it == m_subscribers.end())
            return;

        auto& subscribers = it->second;
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                [&active_subscribers](const std::weak_ptr<Subscription>& weak) {
                    if (auto shared = weak.lock()) {
                        active_subscribers.push_back(shared);
                        return false;
                    }
                    return true;
                }),
            subscribers.end());
    }

    for (auto& subscriber : active_subscribers) {
        subscriber->on_event(event);
    }
}

}
