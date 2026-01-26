#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Vruta/EventSource.hpp"

#include "coroutine"

namespace MayaFlux::Kriya {

/**
 * @class EventAwaiter
 * @brief Awaiter for suspending on window events with optional filtering
 *
 * Allows coroutines to suspend until specific window events arrive.
 * Events are filtered by type and/or input key/button, with support for
 * both awaiting any event or specific event types.
 *
 * @note EventAwaiter takes EventFilter by value, so temporary filters
 * created in next_event() and await_event() are safe.
 *
 * Usage Examples:
 * @code
 * // Wait for any event
 * auto event = co_await event_source.next_event();
 *
 * // Wait for specific event type
 * auto event = co_await event_source.await_event(WindowEventType::KEY_PRESSED);
 *
 * // Wait for specific key press (via EventFilter)
 * Vruta::EventFilter filter(IO::Keys::Escape);
 * auto event = co_await EventAwaiter(source, filter);
 *
 * // Manual filter construction for complex queries
 * Vruta::EventFilter filter;
 * filter.event_type = WindowEventType::KEY_PRESSED;
 * filter.key_code = IO::Keys::Space;
 * auto event = co_await EventAwaiter(source, filter);
 * @endcode
 *
 * @see EventSource for creating awaiters
 * @see EventFilter for filter construction
 * @see IO::Keys for key enumeration
 */
class MAYAFLUX_API EventAwaiter {
public:
    EventAwaiter(Vruta::EventSource& source, Vruta::EventFilter filter = {})
        : m_source(source)
        , m_filter(filter)
    {
    }

    ~EventAwaiter();

    EventAwaiter(const EventAwaiter&) = delete;
    EventAwaiter& operator=(const EventAwaiter&) = delete;
    EventAwaiter(EventAwaiter&&) noexcept = default;
    EventAwaiter& operator=(EventAwaiter&&) noexcept = delete;

    /**
     * @brief Check if event already available
     */
    bool await_ready();

    /**
     * @brief Suspend coroutine, register for event notification
     */
    void await_suspend(std::coroutine_handle<> handle);

    /**
     * @brief Resume with event data
     */
    Core::WindowEvent await_resume();

    /**
     * @brief Called by EventSource when event arrives
     */
    void try_resume();

private:
    Vruta::EventSource& m_source;
    Vruta::EventFilter m_filter;
    Core::WindowEvent m_result;
    std::coroutine_handle<> m_handle;
    bool m_is_suspended = false;

    friend class Vruta::EventSource;
};
}
