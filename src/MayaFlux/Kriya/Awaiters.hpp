#pragma once

#include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
#include "MayaFlux/Vruta/Promise.hpp"

namespace MayaFlux::Vruta {
class EventSource;
}

namespace MayaFlux::Kriya {

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
struct MAYAFLUX_API SampleDelay {
    /**
     * @brief Type alias for the coroutine promise type
     *
     * This alias simplifies references to the Vruta::promise_type
     * throughout the Kriya namespace.
     */
    using promise_handle = Vruta::audio_promise;

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
 * @struct BufferDelay
 * @brief Awaiter for suspending until a buffer cycle boundary
 *
 * Works identically to SampleDelay but at buffer cycle granularity.
 * Accumulates cycles in promise.next_buffer_cycle.
 *
 * **Usage:**
 * ```cpp
 * auto routine = []() -> SoundRoutine {
 *     while (true) {
 *         process_buffer();
 *         co_await BufferDelay{2};  // Resume every 2 buffer cycles
 *     }
 * };
 * ```
 */
struct MAYAFLUX_API BufferDelay {
    /**
     * @brief Type alias for the coroutine promise type
     *
     * This alias simplifies references to the Vruta::promise_type
     * throughout the Kriya namespace.
     */
    using promise_handle = Vruta::audio_promise;

    BufferDelay(uint64_t cycles)
        : num_cycles(cycles)
    {
    }

    uint64_t num_cycles;

    [[nodiscard]] constexpr bool await_ready() const noexcept { return num_cycles == 0; }

    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept
    {
        auto& promise = h.promise();
        promise.next_buffer_cycle += num_cycles;
        promise.delay_amount = num_cycles;
        promise.active_delay_context = Vruta::DelayContext::BUFFER_BASED;
    }

    constexpr void await_resume() const noexcept { }
};

/**
 * @struct FrameDelay
 * @brief graphics-domain awaiter for frame-accurate timing delays
 *
 * Future awaiter for visual processing routines that operate at frame rates.
 * This will work with visual_promise types that have next_frame fields.
 */
struct MAYAFLUX_API FrameDelay {
    /**
     * @brief Type alias for the coroutine promise type
     *
     * This alias simplifies references to the Vruta::promise_type
     * throughout the Kriya namespace.
     */
    using promise_handle = Vruta::graphics_promise;

    uint32_t frames_to_wait;

    [[nodiscard]] constexpr bool await_ready() const noexcept
    {
        return frames_to_wait == 0;
    }

    constexpr void await_resume() const noexcept { }

    void await_suspend(std::coroutine_handle<Vruta::graphics_promise> h) noexcept;
};

struct MAYAFLUX_API MultiRateDelay {

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
 * @struct GetPromiseBase
 * @brief Templated awaitable for accessing a coroutine's promise object
 *
 * This template allows coroutines to access their own promise object in a
 * type-safe, domain-agnostic way. Each domain (audio, graphics, complex, event)
 * can instantiate this template with their specific promise type.
 *
 * @tparam PromiseType The promise type to access (audio_promise, graphics_promise, etc.)
 */
template <typename PromiseType>
struct MAYAFLUX_API GetPromiseBase {
    using promise_handle = PromiseType;

    /**
     * @brief Pointer to store the promise object
     *
     * This field is set during await_suspend and returned by
     * await_resume, providing the coroutine with access to its
     * own promise object.
     */
    promise_handle* promise_ptr = nullptr;

    GetPromiseBase() = default;

    [[nodiscard]] inline bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<promise_handle> h) noexcept
    {
        promise_ptr = &h.promise();
        h.promise().active_delay_context = Vruta::DelayContext::AWAIT;
    }

    [[nodiscard]] promise_handle& await_resume() const noexcept
    {
        return *promise_ptr;
    }
};

/**
 * @brief Audio domain promise accessor
 *
 * Usage in SoundRoutine:
 * ```cpp
 * auto routine = []() -> SoundRoutine {
 *     auto& promise = co_await GetPromise{};
 *     // or explicitly: co_await GetAudioPromise{};
 *     // promise is audio_promise&
 * };
 * ```
 */
using GetAudioPromise = GetPromiseBase<Vruta::audio_promise>;

/**
 * @brief Graphics domain promise accessor
 *
 * Usage in GraphicsRoutine:
 * ```cpp
 * auto routine = []() -> GraphicsRoutine {
 *     auto& promise = co_await GetGraphicsPromise{};
 *     // promise is graphics_promise&
 * };
 * ```
 */
using GetGraphicsPromise = GetPromiseBase<Vruta::graphics_promise>;

/**
 * @brief Multi-domain promise accessor
 *
 * Usage in ComplexRoutine:
 * ```cpp
 * auto routine = []() -> ComplexRoutine {
 *     auto& promise = co_await GetComplexPromise{};
 *     // promise is complex_promise&
 * };
 * ```
 */
using GetComplexPromise = GetPromiseBase<Vruta::complex_promise>;

/**
 * @brief Event-driven promise accessor
 *
 * Usage in Event coroutines:
 * ```cpp
 * auto routine = []() -> Event {
 *     auto& promise = co_await GetEventPromise{};
 *     // promise is event_promise&
 * };
 * ```
 */
using GetEventPromise = GetPromiseBase<Vruta::event_promise>;

using GetPromise = GetAudioPromise;

/**
 * @class EventAwaiter
 * @brief Awaiter for suspending on window events
 *
 * Usage:
 *   auto event = co_await window->get_event_source().next_event();
 *   auto key_event = co_await window->get_event_source().await_event(WindowEventType::KEY_PRESSED);
 */
class MAYAFLUX_API EventAwaiter {
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
