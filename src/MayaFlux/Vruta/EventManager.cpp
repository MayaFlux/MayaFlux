#include "EventManager.hpp"

#include <algorithm>

namespace MayaFlux::Vruta {

void EventManager::add_event(const std::shared_ptr<Event>& event, const std::string& name)
{
    if (!event)
        return;

    std::string event_name = name.empty() ? auto_generate_name(event) : name;

    if (!name.empty() && m_named_events.count(event_name)) {
        remove_event(event_name);
    }

    m_events.push_back(event);
    if (!name.empty()) {
        m_named_events[event_name] = event;
    }
}

bool EventManager::cancel_event(const std::string& name)
{
    auto it = m_named_events.find(name);
    if (it == m_named_events.end())
        return false;

    auto event = it->second;
    if (event && event->is_active()) {
        event->set_should_terminate(true);
    }

    m_named_events.erase(it);

    std::erase(m_events, event);

    return true;
}

bool EventManager::cancel_event(const std::shared_ptr<Event>& event)
{

    auto it = std::ranges::find(m_events, event);
    if (it != m_events.end()) {
        if (event && event->is_active()) {
            event->set_should_terminate(true);
        }
        m_events.erase(it);
        return true;
    }

    return false;
}

std::shared_ptr<Event> EventManager::get_event(const std::string& name) const
{
    return find_event_by_name(name);
}

bool EventManager::remove_event(const std::string& name)
{
    auto it = m_named_events.find(name);
    if (it == m_named_events.end())
        return false;

    auto event = it->second;
    m_named_events.erase(it);

    std::erase(m_events, event);
    return true;
}

bool EventManager::has_active_events() const
{
    return std::ranges::any_of(m_events,
        [](const auto& event) {
            return event && event->is_active();
        });
}

u_int64_t EventManager::get_next_event_id() const
{
    return m_next_event_id.fetch_add(1);
}

std::string EventManager::auto_generate_name(const std::shared_ptr<Event>& /*event*/) const
{
    return "event_" + std::to_string(get_next_event_id());
}

std::shared_ptr<Event> EventManager::find_event_by_name(const std::string& name)
{
    auto it = m_named_events.find(name);
    if (it != m_named_events.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Event> EventManager::find_event_by_name(const std::string& name) const
{
    auto it = m_named_events.find(name);
    if (it != m_named_events.end()) {
        return it->second;
    }
    return nullptr;
}

void EventManager::cleanup_completed_events()
{
    std::erase_if(m_events, [](const auto& e) {
        return !e || e->done();
    });

    std::erase_if(m_named_events, [](const auto& pair) {
        return !pair.second || pair.second->done();
    });
}

void EventManager::terminate_all_events()
{
    for (auto& event : m_events) {
        if (event && event->is_active()) {
            event->set_should_terminate(true);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    m_events.clear();
}

}
