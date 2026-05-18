#include "WindowEventSource.hpp"

#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"

namespace MayaFlux::Vruta {

void WindowEventSource::signal(Core::WindowEvent event)
{
    if (auto* kd = std::get_if<Core::WindowEvent::KeyData>(&event.data)) {
        if (event.type == Core::WindowEventType::KEY_PRESSED) {
            m_key_states[kd->key] = true;
        } else if (event.type == Core::WindowEventType::KEY_RELEASED) {
            m_key_states[kd->key] = false;
        }
    }

    if (auto* bd = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data)) {
        if (event.type == Core::WindowEventType::MOUSE_BUTTON_PRESSED) {
            m_button_states[bd->button] = true;
        } else if (event.type == Core::WindowEventType::MOUSE_BUTTON_RELEASED) {
            m_button_states[bd->button] = false;
        }
    }

    if (auto* pd = std::get_if<Core::WindowEvent::MousePosData>(&event.data)) {
        m_mouse_x = pd->x;
        m_mouse_y = pd->y;
    }

    m_pending_events.push(event);
    dispatch(&event);
}

Kriya::WindowEventAwaiter WindowEventSource::next_event()
{
    return Kriya::WindowEventAwaiter { *this, {} };
}

Kriya::WindowEventAwaiter WindowEventSource::await_event(Core::WindowEventType type)
{
    return Kriya::WindowEventAwaiter { *this, type };
}

std::optional<Core::WindowEvent> WindowEventSource::pop_event(const WindowEventFilter& filter)
{
    if (m_pending_events.empty())
        return std::nullopt;

    if (!filter.event_type && !filter.key_code && !filter.button) {
        auto ev = m_pending_events.front();
        m_pending_events.pop();
        return ev;
    }

    std::queue<Core::WindowEvent> tmp;
    std::optional<Core::WindowEvent> result;

    while (!m_pending_events.empty()) {
        auto ev = m_pending_events.front();
        m_pending_events.pop();

        bool matches = true;

        if (filter.event_type && ev.type != *filter.event_type)
            matches = false;

        if (matches && filter.key_code) {
            if (auto* kd = std::get_if<Core::WindowEvent::KeyData>(&ev.data)) {
                matches = kd->key == static_cast<int16_t>(*filter.key_code);
            } else {
                matches = false;
            }
        }

        if (matches && filter.button) {
            if (auto* bd = std::get_if<Core::WindowEvent::MouseButtonData>(&ev.data)) {
                matches = bd->button == static_cast<int>(*filter.button);
            } else {
                matches = false;
            }
        }

        if (matches && !result) {
            result = ev;
        } else {
            tmp.push(ev);
        }
    }

    m_pending_events = std::move(tmp);
    return result;
}

bool WindowEventSource::is_key_pressed(IO::Keys key) const
{
    auto it = m_key_states.find(static_cast<int16_t>(key));
    return it != m_key_states.end() && it->second;
}

bool WindowEventSource::is_mouse_pressed(int button) const
{
    auto it = m_button_states.find(button);
    return it != m_button_states.end() && it->second;
}

std::pair<double, double> WindowEventSource::get_mouse_position() const
{
    return { m_mouse_x, m_mouse_y };
}

} // namespace MayaFlux::Vruta
