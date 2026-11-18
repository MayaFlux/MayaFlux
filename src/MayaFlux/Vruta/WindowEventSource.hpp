#pragma once

#include "EventSource.hpp"

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/IO/Keys.hpp"

#include <deque>

namespace MayaFlux::Kriya {
class WindowEventAwaiter;
}

namespace MayaFlux::Vruta {

// =============================================================================
// EventFilter
// =============================================================================

/**
 * @struct WindowEventFilter
 * @brief Filter criteria for window input events.
 *
 * Multiple criteria are ANDed. An empty filter matches any event. Modifiers,
 * when set, must equal the held Shift, Ctrl, Alt and Super bits of a key or
 * mouse button event; lock bits are ignored, and an event with no modifier
 * field never matches.
 */
struct MAYAFLUX_API WindowEventFilter : public EventFilter {
    std::optional<Core::WindowEventType> event_type;
    std::optional<IO::Keys> key_code;
    std::optional<IO::MouseButtons> button;
    std::optional<IO::Modifiers> modifiers;

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

    /**
     * @brief Constructs a filter matching a key and, if set, its modifiers.
     * @param key Key and modifier requirement to match.
     */
    WindowEventFilter(IO::ModifiedKey key)
        : key_code(key.key)
        , modifiers(key.modifiers)
    {
    }

    /**
     * @brief Whether an event satisfies every criterion that is set.
     */
    [[nodiscard]] bool matches(const Core::WindowEvent& event) const;

    /**
     * @brief Whether the modifier criterion accepts the mods carried by event data.
     *
     * True when no modifiers are required. False when they are required and the
     * data is neither KeyData nor MouseButtonData.
     */
    [[nodiscard]] bool matches_modifiers(const Core::WindowEvent::EventData& data) const;
};

// =============================================================================
// WindowEventSource
// =============================================================================

/**
 * @class WindowEventSource
 * @brief Awaitable stream of window input events.
 *
 * Signals are produced by the window backend's poll(), on the polling thread. Coroutines
 * suspend via WindowEventAwaiter and are resumed synchronously from signal().
 * Every suspended awaiter whose filter matches receives the event, in registration
 * order. An event no awaiter took is queued until one arrives, up to
 * k_max_pending_events per event type; beyond that the oldest queued event of
 * the same type is dropped, so a flood of one type never evicts another.
 * Tracks input state for immediate polling via is_key_pressed(),
 * is_mouse_pressed(), and get_mouse_position().
 *
 * @see EventSource
 * @see WindowEventAwaiter
 * @see EventFilter
 */
class MAYAFLUX_API WindowEventSource : public EventSource {
public:
    /// @brief Most events of one type held for a later awaiter before the oldest of that type is dropped.
    static constexpr size_t k_max_pending_events = 1024;

    WindowEventSource() = default;
    ~WindowEventSource() override = default;

    WindowEventSource(const WindowEventSource&) = delete;
    WindowEventSource& operator=(const WindowEventSource&) = delete;
    WindowEventSource(WindowEventSource&&) noexcept = default;
    WindowEventSource& operator=(WindowEventSource&&) noexcept = default;

    /**
     * @brief Enqueue a window event and resume matching waiters.
     * @param event Event produced by the window backend.
     *
     * Updates input state caches before dispatching.
     */
    void signal(Core::WindowEvent event);

    /**
     * @brief Run an action after the active outermost window-event dispatch.
     * @param action Action to run after event waiters, or immediately when
     *               the source is idle.
     */
    void defer(std::function<void()> action);

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
    void clear() override
    {
        m_pending_events.clear();
        m_pending_counts.fill(0);
    }

    /**
     * @brief Returns true if the given key is currently held.
     * @param key Key to query.
     */
    [[nodiscard]] bool is_key_pressed(IO::Keys key) const;

    /**
     * @brief Modifier state carried by the most recent key or mouse button event.
     *
     * Reflects every modifier key press and release, including the releases a
     * backend emits when focus is lost, so it also answers for events that
     * carry no modifiers of their own, such as scroll and motion. Hold bits
     * are Shift, Ctrl, Alt and Super; lock bits are CapsLock and NumLock.
     */
    [[nodiscard]] IO::Modifiers modifiers() const { return m_modifiers; }

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
    std::deque<Core::WindowEvent> m_pending_events;
    std::array<size_t, 256> m_pending_counts {};
    std::unordered_map<int16_t, bool> m_key_states;
    std::unordered_map<int, bool> m_button_states;
    IO::Modifiers m_modifiers { IO::Modifiers::None };
    double m_mouse_x {};
    double m_mouse_y {};
    std::vector<std::function<void()>> m_deferred_actions;
    size_t m_signal_depth {};
    size_t m_resumed {};

    /**
     * @brief Removes and returns the first pending event matching the filter.
     * @param filter Filter criteria.
     */
    std::optional<Core::WindowEvent> pop_event(const WindowEventFilter& filter);

    friend class Kriya::WindowEventAwaiter;
};

} // namespace MayaFlux::Vruta
