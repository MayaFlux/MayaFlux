#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Vruta/WindowEventSource.hpp"

#include <coroutine>

namespace MayaFlux::Kriya {

// =============================================================================
// EventAwaiter - base
// =============================================================================

/**
 * @class EventAwaiter
 * @brief Abstract base for all event-driven awaiters.
 *
 * Provides the interface EventSource uses to manage its waiter list
 * without knowing the payload type or filter semantics of concrete awaiters.
 *
 * @see WindowEventAwaiter
 * @see SignalAwaiter
 */
class MAYAFLUX_API EventAwaiter {
public:
    virtual ~EventAwaiter() = default;

    EventAwaiter() = default;
    EventAwaiter(const EventAwaiter&) = delete;
    EventAwaiter& operator=(const EventAwaiter&) = delete;
    EventAwaiter(EventAwaiter&&) noexcept = default;
    EventAwaiter& operator=(EventAwaiter&&) noexcept = default;

    /**
     * @brief Called by the source to attempt resumption of the suspended coroutine.
     *
     * @param event Type-erased pointer to the candidate signal value.
     */
    virtual void try_resume(const void* event) = 0;

    /**
     * @brief Returns true if the candidate signal passes this awaiter's filter.
     *
     * @param event Type-erased pointer to the candidate signal value.
     */
    [[nodiscard]] virtual bool filter_matches(const void* event) const = 0;

protected:
    std::coroutine_handle<> m_handle;
    bool m_is_suspended {};
};

// =============================================================================
// WindowEventAwaiter
// =============================================================================

/**
 * @class WindowEventAwaiter
 * @brief Awaiter for suspending on GLFW window input events with optional filtering.
 *
 * Payload type is Core::WindowEvent. Filter criteria are WindowEventType,
 * IO::Keys, and IO::MouseButtons via EventFilter. Works with any coroutine
 * type: SoundRoutine, GraphicsRoutine, Event, ComplexRoutine.
 *
 * Multiple awaiters may register against the same WindowEventSource simultaneously.
 * The source must outlive any suspended awaiter.
 *
 * @code
 * auto ev = co_await source.next_event();
 * auto ev = co_await source.await_event(WindowEventType::KEY_PRESSED);
 *
 * Vruta::EventFilter f(IO::Keys::Escape);
 * auto ev = co_await WindowEventAwaiter(source, f);
 * @endcode
 *
 * @see WindowEventSource
 * @see EventFilter
 */
class MAYAFLUX_API WindowEventAwaiter : public EventAwaiter {
public:
    explicit WindowEventAwaiter(Vruta::WindowEventSource& source, Vruta::WindowEventFilter filter = {})
        : m_source(source)
        , m_filter(std::move(filter))
    {
    }

    ~WindowEventAwaiter() override;

    WindowEventAwaiter(const WindowEventAwaiter&) = delete;
    WindowEventAwaiter& operator=(const WindowEventAwaiter&) = delete;
    WindowEventAwaiter(WindowEventAwaiter&&) noexcept = default;
    WindowEventAwaiter& operator=(WindowEventAwaiter&&) noexcept = delete;

    /**
     * @brief Returns true if a matching event is already queued; stores it if so.
     */
    bool await_ready();

    /**
     * @brief Registers with the source and suspends the coroutine.
     * @param handle Type-erased coroutine handle.
     */
    void await_suspend(std::coroutine_handle<> handle);

    /**
     * @brief Returns the event that caused resumption.
     */
    Core::WindowEvent await_resume();

    /**
     * @brief Casts event to Core::WindowEvent, checks filter, resumes if matched.
     * @param event Type-erased pointer to Core::WindowEvent.
     */
    void try_resume(const void* event) override;

    /**
     * @brief Casts event to Core::WindowEvent and checks against stored filter.
     * @param event Type-erased pointer to Core::WindowEvent.
     */
    [[nodiscard]] bool filter_matches(const void* event) const override;

private:
    Vruta::WindowEventSource& m_source;
    Vruta::WindowEventFilter m_filter;
    Core::WindowEvent m_result;

    friend class Vruta::WindowEventSource;
};

} // namespace MayaFlux::Kriya
