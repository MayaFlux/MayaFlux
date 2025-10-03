#pragma once

#include "Promise.hpp"

namespace MayaFlux::Vruta {

/**
 * @class Event
 * @brief Coroutine type for event-driven suspension
 *
 * Events suspend on EventAwaiter (window events, custom signals, etc.)
 * and are resumed directly by EventSource when events occur.
 *
 * Unlike AudioRoutine/GraphicsRoutine, there is no periodic processing.
 * Events are "fire-and-forget" - they run until completion or
 * explicit cancellation.
 *
 * Usage:
 *   auto handler = [](Window* w) -> Event {
 *       while (true) {
 *           auto event = co_await w->get_event_source().next_event();
 *           // Handle event
 *       }
 *   };
 */
class Event {
public:
    using promise_type = MayaFlux::Vruta::event_promise;

    /**
     * @brief Constructs a Event from a coroutine handle
     * @param h Handle to the coroutine
     *
     * Creates a Event that wraps and manages the given coroutine.
     * This is typically called by the compiler-generated code when a
     * coroutine function returns a Event.
     */
    Event(std::coroutine_handle<promise_type> h);

    /**
     * @brief Copy constructor
     * @param other Event to copy
     *
     * Creates a new Event that shares ownership of the same
     * underlying coroutine. This allows multiple schedulers or containers
     * to reference the same task.
     */
    Event(const Event& other);

    /**
     * @brief Copy assignment operator
     * @param other Event to copy
     * @return Reference to this Event
     *
     * Assigns this Event to share ownership of the same
     * underlying coroutine as the other Event.
     */
    Event& operator=(const Event& other);

    /**
     * @brief Move constructor
     * @param other Event to move from
     *
     * Transfers ownership of the coroutine from other to this Event.
     * After the move, other no longer references any coroutine.
     */
    Event(Event&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other Event to move from
     * @return Reference to this Event
     *
     * Transfers ownership of the coroutine from other to this Event.
     * After the move, other no longer references any coroutine.
     */
    Event& operator=(Event&& other) noexcept;

    /**
     * @brief Destructor
     *
     * Destroys the Event routine and releases its reference to the
     * underlying coroutine. If this is the last reference, the
     * coroutine frame is destroyed.
     */
    virtual ~Event();

    /**
     * @brief Get the processing token that determines how this routine should be scheduled
     * @return The processing token indicating the scheduling domain
     */
    [[nodiscard]] virtual ProcessingToken get_processing_token() const;

    /**
     * @brief Checks if the coroutine is still active
     * @return True if the coroutine is active, false if it's completed or invalid
     *
     * An active coroutine has not yet completed its execution and can
     * be resumed. Inactive coroutines have either completed or were
     * never properly initialized.
     */
    [[nodiscard]] virtual bool is_active() const;

    void resume();

    [[nodiscard]] bool done() const;

    [[nodiscard]] std::coroutine_handle<promise_type> get_handle() const;

    /**
     * @brief Get auto_resume flag from promise
     * @return True if coroutine should be automatically resumed
     */
    [[nodiscard]] virtual bool get_auto_resume() const
    {
        return m_handle.promise().auto_resume;
    }
    /**
     * @brief Set auto_resume flag in promise
     * @param auto_resume Whether the coroutine should be automatically resumed
     */
    virtual void set_auto_resume(bool auto_resume)
    {
        m_handle.promise().auto_resume = auto_resume;
    }

    /**
     * @brief Get should_terminate flag from promise
     * @return True if coroutine should be terminated
     */
    [[nodiscard]] virtual bool get_should_terminate() const
    {
        return m_handle.promise().should_terminate;
    }

    /**
     * @brief Set should_terminate flag in promise
     * @param should_terminate Whether the coroutine should be terminated
     */
    virtual void set_should_terminate(bool should_terminate)
    {
        m_handle.promise().should_terminate = should_terminate;
    }

    /**
     * @brief Updates multiple named parameters in the coroutine's state
     * @param args Variable number of key-value pairs to update
     *
     * Provides a convenient way to update multiple state values at once.
     * This is useful for configuring a coroutine before or during execution.
     *
     * Example:
     * ```cpp
     * routine.update_params("duration", 2.0f, "amplitude", 0.8f);
     * ```
     */
    template <typename... Args>
    inline void update_params(Args... args)
    {
        update_params_impl(std::forward<Args>(args)...);
    }

    /**
     * @brief Sets a named state value in the coroutine
     * @param key Name of the state value
     * @param value Value to store
     *
     * Stores a value in the coroutine's state dictionary under the given key.
     * This allows the coroutine to maintain state between suspensions and
     * enables external code to influence the coroutine's behavior.
     */
    template <typename T>
    inline void set_state(const std::string& key, T value)
    {
        set_state_impl(key, std::move(value));
    }

    /**
     * @brief Gets a named state value from the coroutine
     * @param key Name of the state value to retrieve
     * @return Pointer to the stored value, or nullptr if not found
     *
     * Retrieves a previously stored value from the coroutine's state
     * dictionary. Returns nullptr if the key doesn't exist or the
     * stored type doesn't match the requested type.
     */
    template <typename T>
    inline T* get_state(const std::string& key)
    {
        return get_state_impl<T>(key);
    }

protected:
    virtual void set_state_impl(const std::string& key, std::any value);

    virtual void* get_state_impl_raw(const std::string& key);

    /**
     * @brief Implementation helper for get_state
     * @param key Name of the state value to retrieve
     * @return Pointer to the stored value, or nullptr if not found or type mismatch
     */
    template <typename T>
    T* get_state_impl(const std::string& key)
    {
        void* raw_ptr = get_state_impl_raw(key);
        if (!raw_ptr)
            return nullptr;

        try {
            return std::any_cast<T>(static_cast<std::any*>(raw_ptr));
        } catch (const std::bad_any_cast&) {
            return nullptr;
        }
    }

    /**
     * brief Implementation helper for update_params
     */
    virtual void update_params_impl() { }

    /**
     * @brief Implementation helper for update_params
     * @param promise The promise object to update
     * @param key Name of the state value
     * @param value Value to store
     * @param args Remaining key-value pairs to process
     *
     * Recursive template function that processes each key-value pair
     * in the update_params variadic argument list.
     */
    template <typename T, typename... Args>
    void update_params_impl(const std::string& key, T value, Args... args)
    {
        set_state(key, std::move(value));
        if constexpr (sizeof...(args) > 0) {
            update_params_impl(std::forward<Args>(args)...);
        }
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

}
