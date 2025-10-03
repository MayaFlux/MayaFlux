#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include <queue>

namespace MayaFlux::Kriya {
class EventAwaiter;
}

namespace MayaFlux::Vruta {

/**
 * @class EventSource
 * @brief Awaitable event stream for window events
 *
 * Unlike clocks (SampleClock, FrameClock) which tick periodically,
 * EventSource is signaled when discrete window events occur.
 * Coroutines can suspend until events arrive.
 */
class EventSource {
public:
    EventSource() = default;
    ~EventSource() = default;

    // Non-copyable, movable
    EventSource(const EventSource&) = delete;
    EventSource& operator=(const EventSource&) = delete;
    EventSource(EventSource&&) noexcept = default;
    EventSource& operator=(EventSource&&) noexcept = default;

    /**
     * @brief Signals that an event occurred
     * @param event The event data
     *
     * Called by GLFW callbacks. Queues event and resumes waiting coroutines.
     */
    void signal(Core::WindowEvent event);

    /**
     * @brief Creates awaiter for next event (any type)
     */
    Kriya::EventAwaiter next_event();

    /**
     * @brief Creates awaiter for specific event type
     */
    Kriya::EventAwaiter await_event(Core::WindowEventType type);

    /**
     * @brief Checks if events are pending
     */
    [[nodiscard]] bool has_pending() const { return !m_pending_events.empty(); }

    /**
     * @brief Gets number of pending events
     */
    [[nodiscard]] size_t pending_count() const { return m_pending_events.size(); }

    /**
     * @brief Clears all pending events
     */
    void clear() { m_pending_events = {}; }

private:
    std::queue<Core::WindowEvent> m_pending_events;
    std::vector<Kriya::EventAwaiter*> m_waiters;

    // Used by EventAwaiter
    std::optional<Core::WindowEvent> pop_event(std::optional<Core::WindowEventType> filter);
    void register_waiter(Kriya::EventAwaiter* awaiter);
    void unregister_waiter(Kriya::EventAwaiter* awaiter);

    friend class Kriya::EventAwaiter;
};

}
