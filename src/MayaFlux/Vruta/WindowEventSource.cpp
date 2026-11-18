#include "WindowEventSource.hpp"

#include "MayaFlux/Kriya/Awaiters/EventAwaiter.hpp"

namespace MayaFlux::Vruta {

void WindowEventSource::signal(Core::WindowEvent event)
{
    if (auto* kd = std::get_if<Core::WindowEvent::KeyData>(&event.data)) {
        m_modifiers = kd->mods;
        if (event.type == Core::WindowEventType::KEY_PRESSED) {
            m_key_states[kd->key] = true;
        } else if (event.type == Core::WindowEventType::KEY_RELEASED) {
            m_key_states[kd->key] = false;
        }
    }

    if (auto* bd = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data)) {
        m_modifiers = bd->mods;
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

    const size_t resumed_before = m_resumed;
    ++m_signal_depth;
    dispatch(&event);
    --m_signal_depth;
    if (m_resumed == resumed_before) {
        m_pending_events.push_back(event);
        auto& held = m_pending_counts[static_cast<uint8_t>(event.type)];
        if (++held > k_max_pending_events) {
            auto oldest = std::ranges::find(m_pending_events, event.type, &Core::WindowEvent::type);
            m_pending_events.erase(oldest);
            --held;
        }
    }

    if (m_signal_depth != 0 || m_deferred_actions.empty())
        return;

    std::vector<std::function<void()>> deferred_actions;
    deferred_actions.swap(m_deferred_actions);
    for (auto& action : deferred_actions)
        action();
}

void WindowEventSource::defer(std::function<void()> action)
{
    if (!action)
        return;

    if (m_signal_depth == 0) {
        action();
        return;
    }

    m_deferred_actions.push_back(std::move(action));
}

Kriya::WindowEventAwaiter WindowEventSource::next_event()
{
    return Kriya::WindowEventAwaiter { *this, {} };
}

Kriya::WindowEventAwaiter WindowEventSource::await_event(Core::WindowEventType type)
{
    return Kriya::WindowEventAwaiter { *this, type };
}

bool WindowEventFilter::matches_modifiers(const Core::WindowEvent::EventData& data) const
{
    if (!modifiers)
        return true;

    IO::Modifiers mods = IO::Modifiers::None;
    if (const auto* key_data = std::get_if<Core::WindowEvent::KeyData>(&data)) {
        mods = key_data->mods;
    } else if (const auto* button_data = std::get_if<Core::WindowEvent::MouseButtonData>(&data)) {
        mods = button_data->mods;
    } else {
        return false;
    }

    constexpr auto hold = IO::Modifiers::Shift | IO::Modifiers::Ctrl
        | IO::Modifiers::Alt | IO::Modifiers::Super;
    return (mods & hold) == *modifiers;
}

bool WindowEventFilter::matches(const Core::WindowEvent& event) const
{
    if (event_type && event.type != *event_type)
        return false;

    if (key_code) {
        const auto* key_data = std::get_if<Core::WindowEvent::KeyData>(&event.data);
        if (!key_data || key_data->key != static_cast<int16_t>(*key_code))
            return false;
    }

    if (button) {
        const auto* button_data = std::get_if<Core::WindowEvent::MouseButtonData>(&event.data);
        if (!button_data || button_data->button != static_cast<int>(*button))
            return false;
    }

    return matches_modifiers(event.data);
}

std::optional<Core::WindowEvent> WindowEventSource::pop_event(const WindowEventFilter& filter)
{
    if (m_pending_events.empty())
        return std::nullopt;

    if (!filter.event_type && !filter.key_code && !filter.button && !filter.modifiers) {
        auto ev = m_pending_events.front();
        m_pending_events.pop_front();
        --m_pending_counts[static_cast<uint8_t>(ev.type)];
        return ev;
    }

    std::deque<Core::WindowEvent> tmp;
    std::optional<Core::WindowEvent> result;

    while (!m_pending_events.empty()) {
        auto ev = m_pending_events.front();
        m_pending_events.pop_front();

        if (!result && filter.matches(ev)) {
            result = ev;
        } else {
            tmp.push_back(ev);
        }
    }

    m_pending_events = std::move(tmp);
    if (result)
        --m_pending_counts[static_cast<uint8_t>(result->type)];
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
