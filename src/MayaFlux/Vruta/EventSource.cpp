#include "EventSource.hpp"

#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"

namespace MayaFlux::Vruta {

void EventSource::signal(Core::WindowEvent event)
{
    if (auto* key_data = std::get_if<Core::WindowEvent::KeyData>(&event.data)) {
        if (event.type == Core::WindowEventType::KEY_PRESSED) {
            m_key_states[key_data->key] = true;
        } else if (event.type == Core::WindowEventType::KEY_RELEASED) {
            m_key_states[key_data->key] = false;
        }
    }

    if (auto* button_data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data)) {
        if (event.type == Core::WindowEventType::MOUSE_BUTTON_PRESSED) {
            m_button_states[button_data->button] = true;
        } else if (event.type == Core::WindowEventType::MOUSE_BUTTON_RELEASED) {
            m_button_states[button_data->button] = false;
        }
    }

    if (auto* pos_data = std::get_if<Core::WindowEvent::MousePosData>(&event.data)) {
        m_mouse_x = pos_data->x;
        m_mouse_y = pos_data->y;
    }

    m_pending_events.push(event);

    auto waiters = m_waiters;
    for (auto* awaiter : waiters) {
        awaiter->try_resume();
    }
}

Kriya::EventAwaiter EventSource::next_event()
{
    return Kriya::EventAwaiter { *this, {} };
}

Kriya::EventAwaiter EventSource::await_event(Core::WindowEventType type)
{
    return Kriya::EventAwaiter { *this, type };
}

std::optional<Core::WindowEvent> EventSource::pop_event(const EventFilter& filter)
{
    if (m_pending_events.empty()) {
        return std::nullopt;
    }

    if (!filter.event_type && !filter.key_code && !filter.button) {
        auto event = m_pending_events.front();
        m_pending_events.pop();
        return event;
    }

    std::queue<Core::WindowEvent> temp_queue;
    std::optional<Core::WindowEvent> result;

    while (!m_pending_events.empty()) {
        auto event = m_pending_events.front();
        m_pending_events.pop();

        bool matches = true;

        if (filter.event_type && event.type != *filter.event_type) {
            matches = false;
        }

        if (matches && filter.key_code) {
            auto key_code = static_cast<int16_t>(*filter.key_code);
            if (auto key_data = std::get_if<Core::WindowEvent::KeyData>(&event.data)) {
                if (key_data->key != key_code) {
                    matches = false;
                }
            } else {
                matches = false;
            }
        }

        if (matches && filter.button) {
            auto button_code = static_cast<int>(*filter.button);
            if (auto* button_data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data)) {
                if (button_data->button != button_code) {
                    matches = false;
                }
            } else {
                matches = false;
            }
        }

        if (matches && !result) {
            result = event;
        } else {
            temp_queue.push(event);
        }
    }

    m_pending_events = std::move(temp_queue);
    return result;
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

bool EventSource::is_key_pressed(IO::Keys key) const
{
    auto key_int = static_cast<int16_t>(key);
    auto it = m_key_states.find(key_int);
    return it != m_key_states.end() && it->second;
}

bool EventSource::is_mouse_pressed(int button) const
{
    auto it = m_button_states.find(button);
    return it != m_button_states.end() && it->second;
}

std::pair<double, double> EventSource::get_mouse_position() const
{
    return { m_mouse_x, m_mouse_y };
}

} // namespace MayaFlux::Vruta
