#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Vruta/Promise.hpp"

namespace MayaFlux::Vruta {
class EventSource;
}

namespace MayaFlux::Kriya {

/**
 * @brief Type alias for the coroutine promise type
 *
 * This alias simplifies references to the Vruta::promise_type
 * throughout the Kriya namespace.
 */
using promise_handle = Vruta::audio_promise;

/**
 * @struct SampleDelay
 * @brief Awaitable object for precise sample-accurate timing delays
 *
 * SampleDelay is the primary timing mechanism in the computational coroutine system.
 * When a coroutine co_awaits a SampleDelay, it suspends execution until
 * exactly the specified number of discrete time units (samples) have been processed.
 *
 * This provides deterministic timing for computational events, which is essential
 * for applications where precise temporal relationships are critical. Unlike
 * system-time-based delays which can drift due to processing load, SampleDelay guarantees
 * that operations occur at exact positions in the discrete time continuum.
 *
 * Example usage:
 * ```cpp
 * // Wait for exactly 4410 time units (100ms at 44.1kHz sample rate)
 * co_await SampleDelay{4410};
 * ```
 *
 * SampleDelay is the foundation for all timing in the computational engine, enabling
 * precise sequencing, modulation, and synchronization of events across multiple domains
 * (signal processing, visual rendering, data transformation, physical modeling, etc.).
 */
struct SampleDelay {
    /**
     * @brief Number of time units to wait before resuming the coroutine
     *
     * This value is added to the coroutine's next_sample field when
     * await_suspend is called, scheduling the coroutine to resume
     * after exactly this many discrete time units have been processed.
     */
    uint64_t samples_to_wait;

    SampleDelay(uint64_t samples)
        : samples_to_wait(samples)
    {
    }

    /**
     * @brief Checks if the delay should be bypassed
     * @return True if no delay is needed, false otherwise
     *
     * If samples_to_wait is 0, the coroutine continues execution
     * without suspending. This optimization avoids the overhead
     * of suspension for zero-length delays.
     */
    [[nodiscard]] inline bool await_ready() const { return samples_to_wait == 0; }

    /**
     * @brief Called when the coroutine is resumed after the delay
     *
     * This method is called when the scheduler resumes the coroutine
     * after the specified delay. It doesn't need to do anything since
     * the delay itself is the only effect needed.
     */
    inline void await_resume() { };

    /**
     * @brief Schedules the coroutine to resume after the delay
     * @param h Handle to the suspended coroutine
     *
     * This method is called when the coroutine suspends. It updates
     * the coroutine's next_sample field to schedule it for resumption
     * after the specified number of time units have been processed.
     */
    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept;
};

/**
 * @struct FrameDelay
 * @brief graphics-domain awaiter for frame-accurate timing delays
 *
 * Future awaiter for visual processing routines that operate at frame rates.
 * This will work with visual_promise types that have next_frame fields.
 */
struct FrameDelay {
    uint32_t frames_to_wait;

    [[nodiscard]] constexpr bool await_ready() const noexcept
    {
        return frames_to_wait == 0;
    }

    constexpr void await_resume() const noexcept { }

    void await_suspend(std::coroutine_handle<Vruta::graphics_promise> h) noexcept;
};

struct MultiRateDelay {
    uint64_t samples_to_wait;
    uint32_t frames_to_wait;

    [[nodiscard]] constexpr bool await_ready() const noexcept
    {
        return samples_to_wait == 0 && frames_to_wait == 0;
    }

    constexpr void await_resume() const noexcept { }

    void await_suspend(std::coroutine_handle<Vruta::complex_promise> h) noexcept;
};

/**
 * @struct GetPromise
 * @brief Awaitable object for accessing the coroutine's promise object
 *
 * GetPromise provides a way for a coroutine to access its own promise
 * object, which contains the coroutine's state and control mechanisms.
 * This is particularly useful for computational processes that need to
 * store state between suspensions or configure their own execution behavior.
 *
 * When a coroutine co_awaits GetPromise, it briefly suspends and then
 * immediately resumes with a reference to its promise object. This
 * allows the coroutine to access and modify its own state storage,
 * timing parameters, and control flags.
 *
 * Example usage:
 * ```cpp
 * auto& promise = co_await GetPromise{};
 * promise.set_state("parameter", 0.5f);
 * ```
 *
 * This pattern enables coroutines to be self-configuring and maintain
 * complex internal state without requiring external management, supporting
 * the development of autonomous computational agents within the system.
 */
struct GetPromise {
    /**
     * @brief Pointer to store the promise object
     *
     * This field is set during await_suspend and returned by
     * await_resume, providing the coroutine with access to its
     * own promise object.
     */
    promise_handle* promise_ptr = nullptr;

    /**
     * @brief Default constructor
     *
     * Creates a new GetPromise awaitable with a null promise pointer.
     * The pointer will be set when await_suspend is called.
     */
    GetPromise() = default;

    /**
     * @brief Checks if the awaitable is already complete
     * @return Always false, as we always need to suspend to get the promise
     *
     * This method always returns false to ensure that the coroutine
     * suspends so that await_suspend can capture the promise pointer.
     */
    [[nodiscard]] inline bool await_ready() const noexcept { return false; }

    /**
     * @brief Captures the promise object when the coroutine suspends
     * @param h Handle to the suspended coroutine
     *
     * This method is called when the coroutine suspends. It stores
     * a pointer to the coroutine's promise object for later retrieval.
     */
    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept;

    /**
     * @brief Returns the captured promise object when the coroutine resumes
     * @return Reference to the coroutine's promise object
     *
     * This method is called when the coroutine resumes. It returns a
     * reference to the promise object that was captured during suspension.
     */
    [[nodiscard]] promise_handle& await_resume() const noexcept;
};

/**
 * @class EventAwaiter
 * @brief Awaiter for suspending on window events
 *
 * Usage:
 *   auto event = co_await window->get_event_source().next_event();
 *   auto key_event = co_await window->get_event_source().await_event(WindowEventType::KEY_PRESSED);
 */
class EventAwaiter {
public:
    EventAwaiter(Vruta::EventSource& source, std::optional<Core::WindowEventType> filter = std::nullopt)
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
    std::optional<Core::WindowEventType> m_filter;
    Core::WindowEvent m_result;
    std::coroutine_handle<> m_handle;
    bool m_is_suspended = false;

    friend class Vruta::EventSource;
};
}
