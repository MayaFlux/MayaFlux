#pragma once

#include "Promise.hpp"

namespace MayaFlux::Vruta {

/**
 * @class Routine
 * @brief Base class for all coroutine types in the MayaFlux engine
 *
 * This abstract base class provides the common interface and functionality
 * for all types of coroutines, regardless of their processing domain.
 * It enables polymorphic handling of different routine types while maintaining
 * type safety through the template-based promise system.
 */
class MAYAFLUX_API Routine {
public:
    /**
     * @brief Destructor
     *
     * Destroys the Routine and releases its reference to the
     * underlying coroutine. If this is the last reference, the
     * coroutine frame is destroyed.
     */
    virtual ~Routine() = default;

    /**
     * @brief Get the processing token that determines how this routine should be scheduled
     * @return The processing token indicating the scheduling domain
     */
    [[nodiscard]] virtual ProcessingToken get_processing_token() const = 0;

    /**
     * @brief Initializes the coroutine's state for execution
     * @param current_sample Current sample position in the timeline
     * @return True if initialization succeeded, false otherwise
     *
     * Prepares the coroutine for execution by setting its initial
     * sample position and other state. This should be called before
     * the first attempt to resume the coroutine.
     */
    virtual bool initialize_state(uint64_t current_context = 0U) = 0;

    /**
     * @brief Checks if the coroutine is still active
     * @return True if the coroutine is active, false if it's completed or invalid
     *
     * An active coroutine has not yet completed its execution and can
     * be resumed. Inactive coroutines have either completed or were
     * never properly initialized.
     */
    [[nodiscard]] virtual bool is_active() const = 0;

    /**
     * @brief Gets the sample position when this routine should next execute
     * @return Sample position for next execution, or uint64_MAX if inactive
     *
     * This method is crucial for sample-accurate scheduling. It returns
     * the exact sample position when the coroutine should be resumed next.
     * The scheduler uses this to determine when to call try_resume().
     */
    [[nodiscard]] virtual inline uint64_t next_execution() const = 0;

    /**
     * @brief Attempts to resume the coroutine if it's ready to execute
     * @param current_sample Current sample position in the timeline
     * @return True if the coroutine was resumed, false otherwise
     *
     * This method checks if the current sample position has reached or
     * passed the coroutine's next execution time. If so, it resumes
     * the coroutine, allowing it to execute until its next suspension
     * point or completion.
     */
    virtual bool try_resume(uint64_t current_context) = 0;

    /**
     * @brief Attempts to resume the coroutine with explicit temporal context
     * @param current_value Current position in the timeline (samples, frames, cycles, etc.)
     * @param context The temporal context being processed
     * @return True if the coroutine was resumed, false otherwise
     *
     * This context-aware resume method allows different temporal mechanisms
     * to coexist within the same processing token. For example, both sample-based
     * and buffer-cycle-based delays can use SAMPLE_ACCURATE token without
     * interfering with each other.
     *
     * The default implementation delegates to try_resume(uint64_t) for backward
     * compatibility. Derived classes can override to implement context-specific
     * resumption logic.
     */
    virtual bool try_resume_with_context(uint64_t current_value, DelayContext /*context*/)
    {
        return try_resume(current_value);
    }

    /**
     * @brief Check if the routine should synchronize with a clock
     * @return True if the routine requires clock synchronization
     */
    [[nodiscard]] virtual bool requires_clock_sync() const = 0;

    /**
     * @brief Restarts the coroutine from the beginning
     * @return True if restart succeeded, false otherwise
     *
     * Resets the coroutine to its initial state and prepares it for
     * execution from the beginning. This allows reusing the same
     * coroutine logic multiple times without creating a new coroutine.
     */
    virtual bool restart() = 0;

    /**
     * @brief Get auto_resume flag from promise
     * @return True if coroutine should be automatically resumed
     */
    [[nodiscard]] virtual bool get_auto_resume() const = 0;

    /**
     * @brief Set auto_resume flag in promise
     * @param auto_resume Whether the coroutine should be automatically resumed
     */
    virtual void set_auto_resume(bool auto_resume) = 0;

    /**
     * @brief Get should_terminate flag from promise
     * @return True if coroutine should be terminated
     */
    [[nodiscard]] virtual bool get_should_terminate() const = 0;

    /**
     * @brief Set should_terminate flag in promise
     * @param should_terminate Whether the coroutine should be terminated
     */
    virtual void set_should_terminate(bool should_terminate) = 0;

    /**
     * @brief Get sync_to_clock flag from promise
     * @return True if coroutine should synchronize with clock
     */
    [[nodiscard]] virtual bool get_sync_to_clock() const = 0;

    // Domain-specific timing methods (return 0/false for unsupported domains)
    /**
     * @brief Get next sample execution time (audio domain)
     * @return Sample position for next execution, or 0 if not audio domain
     */
    [[nodiscard]] virtual uint64_t get_next_sample() const = 0;

    /**
     * @brief Set next sample execution time (audio domain)
     * @param next_sample Sample position for next execution
     */
    virtual void set_next_sample(uint64_t next_sample) = 0;

    /**
     * @brief Get next frame execution time (graphics domain)
     * @return Frame position for next execution, or 0 if not graphics domain
     */
    [[nodiscard]] virtual uint64_t get_next_frame() const = 0;

    /**
     * @brief Set next frame execution time (graphics domain)
     * @param next_frame Frame position for next execution
     */
    virtual void set_next_frame(uint64_t next_frame) = 0;

    /**
     * @brief Get the active delay context for this routine
     * @return Current delay context, or NONE if not waiting
     */
    [[nodiscard]] virtual DelayContext get_delay_context() const { return DelayContext::NONE; }

    /**
     * @brief Set the active delay context for this routine
     * @param context New delay context
     */
    virtual void set_delay_context(DelayContext /* Context */) { /* no-op in base */ }

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
    virtual void set_state_impl(const std::string& key, std::any value) = 0;
    virtual void* get_state_impl_raw(const std::string& key) = 0;

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
};

/**
 * @class SoundRoutine
 * @brief A C++20 coroutine-based audio processing task with sample-accurate timing
 *
 * SoundRoutine encapsulates a coroutine that can execute audio processing logic
 * with sample-accurate timing. It provides a powerful abstraction for writing
 * time-based audio code that appears sequential but executes asynchronously
 * in perfect sync with the audio timeline.
 *
 * Key features:
 * - Sample-accurate timing for precise audio scheduling
 * - State persistence between suspensions and resumptions
 * - Automatic management of coroutine lifetime
 * - Ability to restart and reschedule tasks
 * - Dynamic parameter updates during execution
 * - Named state storage for flexible data management
 *
 * This implementation leverages C++20 coroutines to create a cooperative
 * multitasking system specifically designed for audio processing. Each routine
 * can suspend itself at precise sample positions and be resumed exactly when needed,
 * enabling complex temporal behaviors without blocking the audio thread.
 *
 * Example usage:
 * ```cpp
 * auto fade_in = [](TaskScheduler& scheduler) -> SoundRoutine {
 *     float gain = 0.0f;
 *     for (int i = 0; i < 100; i++) {
 *         gain += 0.01f;
 *         set_volume(gain);
 *         co_await SampleDelay{441}; // Wait 10ms at 44.1kHz
 *     }
 * };
 * ```
 */
class SoundRoutine : public Routine {
public:
    /**
     * @brief Promise type used by this coroutine
     *
     * This is the promise type that manages the coroutine state and
     * provides the co_await, co_yield, and co_return behaviors.
     */
    using promise_type = MayaFlux::Vruta::audio_promise;

    /**
     * @brief Get the processing token that determines how this routine should be scheduled
     * @return The processing token indicating the scheduling domain
     */
    [[nodiscard]] ProcessingToken get_processing_token() const override;

    /**
     * @brief Constructs a SoundRoutine from a coroutine handle
     * @param h Handle to the coroutine
     *
     * Creates a SoundRoutine that wraps and manages the given coroutine.
     * This is typically called by the compiler-generated code when a
     * coroutine function returns a SoundRoutine.
     */
    SoundRoutine(std::coroutine_handle<promise_type> h);

    /**
     * @brief Copy constructor
     * @param other SoundRoutine to copy
     *
     * Creates a new SoundRoutine that shares ownership of the same
     * underlying coroutine. This allows multiple schedulers or containers
     * to reference the same task.
     */
    SoundRoutine(const SoundRoutine& other);

    /**
     * @brief Copy assignment operator
     * @param other SoundRoutine to copy
     * @return Reference to this SoundRoutine
     *
     * Assigns this SoundRoutine to share ownership of the same
     * underlying coroutine as the other SoundRoutine.
     */
    SoundRoutine& operator=(const SoundRoutine& other);

    /**
     * @brief Move constructor
     * @param other SoundRoutine to move from
     *
     * Transfers ownership of the coroutine from other to this SoundRoutine.
     * After the move, other no longer references any coroutine.
     */
    SoundRoutine(SoundRoutine&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other SoundRoutine to move from
     * @return Reference to this SoundRoutine
     *
     * Transfers ownership of the coroutine from other to this SoundRoutine.
     * After the move, other no longer references any coroutine.
     */
    SoundRoutine& operator=(SoundRoutine&& other) noexcept;

    ~SoundRoutine() override;

    [[nodiscard]] bool is_active() const override;

    bool initialize_state(uint64_t current_sample = 0U) override;

    bool try_resume(uint64_t current_context) override;

    bool try_resume_with_context(uint64_t current_value, DelayContext context) override;

    [[nodiscard]] DelayContext get_delay_context() const override
    {
        return m_handle.promise().active_delay_context;
    }

    void set_delay_context(DelayContext context) override
    {
        m_handle.promise().active_delay_context = context;
    }

    bool restart() override;

    [[nodiscard]] uint64_t next_execution() const override;

    [[nodiscard]] bool requires_clock_sync() const override;

    [[nodiscard]] bool get_auto_resume() const override
    {
        return m_handle.promise().auto_resume;
    }

    void set_auto_resume(bool auto_resume) override
    {
        m_handle.promise().auto_resume = auto_resume;
    }

    [[nodiscard]] bool get_should_terminate() const override
    {
        return m_handle.promise().should_terminate;
    }

    void set_should_terminate(bool should_terminate) override
    {
        m_handle.promise().should_terminate = should_terminate;
    }

    [[nodiscard]] bool get_sync_to_clock() const override
    {
        return m_handle.promise().sync_to_clock;
    }

    [[nodiscard]] uint64_t get_next_sample() const override
    {
        return m_handle.promise().next_sample;
    }

    void set_next_sample(uint64_t next_sample) override
    {
        m_handle.promise().next_sample = next_sample;
    }

    [[nodiscard]] uint64_t get_next_frame() const override
    {
        return m_handle.promise().next_buffer_cycle;
    }

    void set_next_frame(uint64_t next_cycle) override
    {
        m_handle.promise().next_buffer_cycle = next_cycle;
    }

    // Non-audio domain methods (return defaults for audio routines)
    // uint64_t get_next_frame() const override { return 0; }
    // void set_next_frame(uint64_t /*next_frame*/) override { /* no-op for audio */ }

protected:
    void set_state_impl(const std::string& key, std::any value) override;
    void* get_state_impl_raw(const std::string& key) override;

private:
    /**
     * @brief Handle to the underlying coroutine
     *
     * This handle provides access to the coroutine frame, allowing
     * the SoundRoutine to resume, suspend, and destroy the coroutine.
     */
    std::coroutine_handle<promise_type> m_handle;
};

/**
 * @class GraphicsRoutine
 * @brief A C++20 coroutine-based graphics processing task with frame-accurate timing
 *
 * GraphicsRoutine encapsulates a coroutine that can execute visual processing logic
 * with frame-accurate timing. It provides the graphics-domain equivalent to SoundRoutine,
 * enabling time-based visual code that appears sequential but executes asynchronously
 * in perfect sync with the frame timeline.
 *
 * Key architectural differences from SoundRoutine:
 * - Timing source: FrameClock (self-driven) vs SampleClock (hardware-driven)
 * - The routine doesn't care HOW the clock advances, only that it gets tick updates
 * - GraphicsRoutine synchronized to frame positions, not sample positions
 * - Works with FrameDelay awaiters instead of SampleDelay
 *
 * Key features:
 * - Frame-accurate timing for precise visual scheduling
 * - State persistence between suspensions and resumptions
 * - Automatic management of coroutine lifetime
 * - Ability to restart and reschedule tasks
 * - Dynamic parameter updates during execution
 * - Named state storage for flexible data management
 *
 * This implementation leverages C++20 coroutines to create a cooperative
 * multitasking system specifically designed for visual processing. Each routine
 * can suspend itself at precise frame positions and be resumed exactly when needed,
 * enabling complex temporal behaviors for animations, visual effects, and
 * data-driven visuals without blocking the graphics thread.
 *
 * Example usage:
 * ```cpp
 * auto fade_animation = [](TaskScheduler& scheduler) -> GraphicsRoutine {
 *     float opacity = 0.0f;
 *     for (int i = 0; i < 60; i++) {  // 60 frames at 60fps = 1 second
 *         opacity += 1.0f / 60.0f;
 *         set_shader_opacity(opacity);
 *         co_await FrameDelay{1};  // Wait exactly 1 frame
 *     }
 * };
 * ```
 */
class GraphicsRoutine : public Routine {
public:
    /**
     * @brief Promise type used by this coroutine
     *
     * This is the promise type that manages the coroutine state and
     * provides the co_await, co_yield, and co_return behaviors for
     * frame-based timing.
     */
    using promise_type = MayaFlux::Vruta::graphics_promise;

    /**
     * @brief Get the processing token that determines how this routine should be scheduled
     * @return FRAME_ACCURATE token indicating frame-based scheduling
     */
    [[nodiscard]] ProcessingToken get_processing_token() const override;

    /**
     * @brief Constructs a GraphicsRoutine from a coroutine handle
     * @param h Handle to the coroutine
     *
     * Creates a GraphicsRoutine that wraps and manages the given coroutine.
     * This is typically called by the compiler-generated code when a
     * coroutine function returns a GraphicsRoutine.
     */
    GraphicsRoutine(std::coroutine_handle<promise_type> h);

    /**
     * @brief Copy constructor
     * @param other GraphicsRoutine to copy
     *
     * Creates a new GraphicsRoutine that shares ownership of the same
     * underlying coroutine. This allows multiple schedulers or containers
     * to reference the same task.
     */
    GraphicsRoutine(const GraphicsRoutine& other);

    /**
     * @brief Copy assignment operator
     * @param other GraphicsRoutine to copy from
     * @return Reference to this GraphicsRoutine
     */
    GraphicsRoutine& operator=(const GraphicsRoutine& other);

    /**
     * @brief Move constructor
     * @param other GraphicsRoutine to move from
     *
     * Transfers ownership of the coroutine from other to this GraphicsRoutine.
     * After the move, other no longer references any coroutine.
     */
    GraphicsRoutine(GraphicsRoutine&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other GraphicsRoutine to move from
     * @return Reference to this GraphicsRoutine
     *
     * Transfers ownership of the coroutine from other to this GraphicsRoutine.
     * After the move, other no longer references any coroutine.
     */
    GraphicsRoutine& operator=(GraphicsRoutine&& other) noexcept;

    ~GraphicsRoutine() override;

    [[nodiscard]] bool is_active() const override;

    bool initialize_state(uint64_t current_frame = 0U) override;

    bool try_resume(uint64_t current_context) override;

    bool try_resume_with_context(uint64_t current_value, DelayContext context) override;

    [[nodiscard]] DelayContext get_delay_context() const override
    {
        return m_handle.promise().active_delay_context;
    }

    void set_delay_context(DelayContext context) override
    {
        m_handle.promise().active_delay_context = context;
    }

    bool restart() override;

    [[nodiscard]] uint64_t next_execution() const override;

    [[nodiscard]] bool requires_clock_sync() const override;

    [[nodiscard]] bool get_auto_resume() const override
    {
        return m_handle.promise().auto_resume;
    }

    void set_auto_resume(bool auto_resume) override
    {
        m_handle.promise().auto_resume = auto_resume;
    }

    [[nodiscard]] bool get_should_terminate() const override
    {
        return m_handle.promise().should_terminate;
    }

    void set_should_terminate(bool should_terminate) override
    {
        m_handle.promise().should_terminate = should_terminate;
    }

    [[nodiscard]] bool get_sync_to_clock() const override
    {
        return m_handle.promise().sync_to_clock;
    }

    [[nodiscard]] uint64_t get_next_frame() const override
    {
        return m_handle.promise().next_frame;
    }

    void set_next_frame(uint64_t next_frame) override
    {
        m_handle.promise().next_frame = next_frame;
    }

    // Non-graphics domain methods (return defaults for graphics routines)
    [[nodiscard]] uint64_t get_next_sample() const override { return 0; }
    void set_next_sample(uint64_t /*next_sample*/) override { /* no-op for graphics */ }

protected:
    void set_state_impl(const std::string& key, std::any value) override;
    void* get_state_impl_raw(const std::string& key) override;

private:
    /**
     * @brief Handle to the underlying coroutine
     *
     * This handle provides access to the coroutine frame, allowing
     * the GraphicsRoutine to resume, suspend, and destroy the coroutine.
     */
    std::coroutine_handle<promise_type> m_handle;
};

/**
 * @class ComplexRoutine
 * @brief Multi-domain coroutine that can handle multiple processing rates
 *
 * ComplexRoutine is designed for coroutines that need to operate across
 * multiple processing domains (audio + visual, for example). It automatically
 * selects the fastest processing rate and manages cross-domain synchronization.
 */
class ComplexRoutine : public Routine {
public:
    [[nodiscard]] ProcessingToken get_processing_token() const override
    {
        return ProcessingToken::MULTI_RATE;
    }

    [[nodiscard]] bool requires_clock_sync() const override
    {
        return true; // Complex routines need clock sync for coordination
    }

    // Promise state access implementations (TODO: implement when complex promise is ready)
    [[nodiscard]] bool get_auto_resume() const override { return true; }
    void set_auto_resume(bool /*auto_resume*/) override { /* TODO */ }
    [[nodiscard]] bool get_should_terminate() const override { return false; }
    void set_should_terminate(bool /*should_terminate*/) override { /* TODO */ }
    [[nodiscard]] bool get_sync_to_clock() const override { return true; }

    // Multi-domain timing implementations (supports both audio and graphics)
    [[nodiscard]] uint64_t get_next_sample() const override { return 0; }
    void set_next_sample(uint64_t /*next_sample*/) override { /* TODO */ }
    [[nodiscard]] uint64_t get_next_frame() const override { return 0; }
    void set_next_frame(uint64_t /*next_frame*/) override { /* TODO */ }

    // TODO: Implement when multi-domain scheduling is ready
    [[nodiscard]] bool is_active() const override { return false; }
    bool initialize_state(uint64_t /*current_context*/ = 0U) override { return false; }
    bool try_resume(uint64_t /*current_context*/) override { return false; }
    bool restart() override { return false; }
    [[nodiscard]] uint64_t next_execution() const override { return 0; }

protected:
    void set_state_impl(const std::string& /*key*/, std::any /*value*/) override { }
    void* get_state_impl_raw(const std::string& /*key*/) override { return nullptr; }

private:
    ProcessingToken m_primary_token = ProcessingToken::SAMPLE_ACCURATE;
    std::vector<ProcessingToken> m_secondary_tokens;
};
}
