#pragma once

#include "EventSource.hpp"

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/IO/Keys.hpp"

#include <queue>

namespace MayaFlux::Kriya {
class WindowEventAwaiter;
}

namespace MayaFlux::Vruta {

// =============================================================================
// EventFilter
// =============================================================================

/**
 * @struct WindowEventFilter
 * @brief Filter criteria for GLFW window input events.
 *
 * Multiple criteria are ANDed. An empty filter matches any event.
 */
struct MAYAFLUX_API WindowEventFilter : public EventFilter {
    std::optional<Core::WindowEventType> event_type;
    std::optional<IO::Keys> key_code;
    std::optional<IO::MouseButtons> button;

    WindowEventFilter() = default;

    /**
     * @brief Constructs a filter matching a specific event type.
     * @param type Event type to match.
     */
    WindowEventFilter(Core::WindowEventType type)
        : event_type(type)
    {
    }

    /**
     * @brief Constructs a filter matching a specific key.
     * @param key Key to match.
     */
    WindowEventFilter(IO::Keys key)
        : key_code(key)
    {
    }
};

// =============================================================================
// WindowEventSource
// =============================================================================

/**
 * @class WindowEventSource
 * @brief Awaitable stream of GLFW window input events.
 *
 * Signals are produced by GLFW callbacks on the main thread. Coroutines
 * suspend via WindowEventAwaiter and are resumed synchronously from signal().
 * Tracks input state for immediate polling via is_key_pressed(),
 * is_mouse_pressed(), and get_mouse_position().
 *
 * @see EventSource
 * @see WindowEventAwaiter
 * @see EventFilter
 */
class MAYAFLUX_API WindowEventSource : public EventSource {
public:
    WindowEventSource() = default;
    ~WindowEventSource() override = default;

    WindowEventSource(const WindowEventSource&) = delete;
    WindowEventSource& operator=(const WindowEventSource&) = delete;
    WindowEventSource(WindowEventSource&&) noexcept = default;
    WindowEventSource& operator=(WindowEventSource&&) noexcept = default;

    /**
     * @brief Enqueue a window event and resume matching waiters.
     * @param event Event produced by the GLFW backend.
     *
     * Updates input state caches before dispatching.
     */
    void signal(Core::WindowEvent event);

    /**
     * @brief Creates an awaiter that resumes on the next event of any type.
     */
    Kriya::WindowEventAwaiter next_event();

    /**
     * @brief Creates an awaiter that resumes on the next event matching a type.
     * @param type Event type to wait for.
     */
    Kriya::WindowEventAwaiter await_event(Core::WindowEventType type);

    [[nodiscard]] bool has_pending() const override { return !m_pending_events.empty(); }
    [[nodiscard]] size_t pending_count() const { return m_pending_events.size(); }
    void clear() override { m_pending_events = {}; }

    /**
     * @brief Returns true if the given key is currently held.
     * @param key Key to query.
     */
    [[nodiscard]] bool is_key_pressed(IO::Keys key) const;

    /**
     * @brief Returns true if the given mouse button is currently held.
     * @param button Button index.
     */
    [[nodiscard]] bool is_mouse_pressed(int button) const;

    /**
     * @brief Returns the last known cursor position in screen coordinates.
     */
    [[nodiscard]] std::pair<double, double> get_mouse_position() const;

private:
    std::queue<Core::WindowEvent> m_pending_events;
    std::unordered_map<int16_t, bool> m_key_states;
    std::unordered_map<int, bool> m_button_states;
    double m_mouse_x {};
    double m_mouse_y {};

    /**
     * @brief Removes and returns the first pending event matching the filter.
     * @param filter Filter criteria.
     */
    std::optional<Core::WindowEvent> pop_event(const WindowEventFilter& filter);

    friend class Kriya::WindowEventAwaiter;
};

} // namespace MayaFlux::Vruta
