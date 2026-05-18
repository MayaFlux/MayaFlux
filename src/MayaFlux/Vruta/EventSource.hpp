#pragma once

namespace MayaFlux::Kriya {
class EventAwaiter;
}

namespace MayaFlux::Vruta {

// =============================================================================
// EventSource - base
// =============================================================================

/**
 * @class EventSource
 * @brief Abstract base for all awaitable signal sources.
 *
 * Manages the waiter list and dispatches to registered awaiters via the
 * type-erased EventAwaiter interface. Has no knowledge of payload type,
 * filter semantics, or threading model.
 *
 * Derived classes call dispatch() with a type-erased pointer to the current
 * signal value. Each registered awaiter receives the pointer and decides
 * independently whether to resume.
 *
 * @see WindowEventSource
 * @see SignalSource
 */
class MAYAFLUX_API EventSource {
public:
    EventSource() = default;
    virtual ~EventSource() = default;

    EventSource(const EventSource&) = delete;
    EventSource& operator=(const EventSource&) = delete;
    EventSource(EventSource&&) noexcept = default;
    EventSource& operator=(EventSource&&) noexcept = default;

    /**
     * @brief Returns true if any signals are buffered and not yet consumed.
     */
    [[nodiscard]] virtual bool has_pending() const = 0;

    /**
     * @brief Discards all buffered signals.
     */
    virtual void clear() = 0;

protected:
    /**
     * @brief Iterates the waiter list, passing the type-erased signal to each.
     *
     * Called by derived classes after committing a new signal value.
     * Awaiters that match resume and unregister themselves.
     *
     * @param event Type-erased pointer to the current signal value.
     */
    void dispatch(const void* event);

    void register_waiter(Kriya::EventAwaiter* awaiter);
    void unregister_waiter(Kriya::EventAwaiter* awaiter);

private:
    std::vector<Kriya::EventAwaiter*> m_waiters;

    friend class Kriya::EventAwaiter;
};

/**
 * @class EventFilter
 * @brief Base for event filters used by EventSources to match signals to awaiters.
 *
 * Concrete filter types (e.g. WindowEventFilter) derive from this and add
 * specific criteria. EventSources use the base pointer to check if a filter is
 * present and to pass it to awaiters, which then downcast as needed.
 *
 * @see WindowEventFilter
 */
class MAYAFLUX_API EventFilter {
public:
    virtual ~EventFilter() = default;
};

} // namespace MayaFlux::Vruta
