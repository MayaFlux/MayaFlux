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
class Routine {
public:
    // using promise_type = MayaFlux::Vruta::routine_promise<Routine>;

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
    virtual ProcessingToken get_processing_token() const = 0;

    /**
     * @brief Initializes the coroutine's state for execution
     * @param current_sample Current sample position in the timeline
     * @return True if initialization succeeded, false otherwise
     *
     * Prepares the coroutine for execution by setting its initial
     * sample position and other state. This should be called before
     * the first attempt to resume the coroutine.
     */
    virtual bool initialize_state(u_int64_t current_context = 0u) = 0;

    /**
     * @brief Checks if the coroutine is still active
     * @return True if the coroutine is active, false if it's completed or invalid
     *
     * An active coroutine has not yet completed its execution and can
     * be resumed. Inactive coroutines have either completed or were
     * never properly initialized.
     */
    virtual bool is_active() const = 0;

    /**
     * @brief Gets the sample position when this routine should next execute
     * @return Sample position for next execution, or UINT64_MAX if inactive
     *
     * This method is crucial for sample-accurate scheduling. It returns
     * the exact sample position when the coroutine should be resumed next.
     * The scheduler uses this to determine when to call try_resume().
     */
    virtual inline u_int64_t next_execution() const = 0;

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
    virtual bool try_resume(u_int64_t current_context) = 0;

    /**
     * @brief Check if the routine should synchronize with a clock
     * @return True if the routine requires clock synchronization
     */
    virtual bool requires_clock_sync() const = 0;

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
        get_state_impl<T>(key);
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
    virtual ProcessingToken get_processing_token() const override;

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

    bool is_active() const override;

    bool initialize_state(u_int64_t current_sample = 0u) override;

    bool try_resume(u_int64_t current_context) override;

    bool restart() override;

    virtual u_int64_t next_execution() const override;

    bool requires_clock_sync() const override;

    /**
     * @brief Gets the underlying coroutine handle
     * @return Handle to the coroutine
     *
     * Provides access to the raw coroutine handle, which can be used
     * for advanced operations not covered by the SoundRoutine interface.
     */
    inline std::coroutine_handle<promise_type> get_handle() { return m_handle; }

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

// TODO: Dummy class, will implement when visual subsystem is ready
class GraphicalRoutine : public Routine {
    ProcessingToken get_processing_token() const override
    {
        return ProcessingToken::FRAME_ACCURATE;
    }

    bool requires_clock_sync() const override
    {
        return true; // Visual routines sync to frame clock
    }

    // TODO: Implement when visual subsystem is ready
    bool is_active() const override { return false; }
    bool initialize_state(u_int64_t current_frame = 0u) override { return false; }
    bool try_resume(u_int64_t current_frame) override { return false; }
    bool restart() override { return false; }
    u_int64_t next_execution() const override { return UINT64_MAX; }

protected:
    void set_state_impl(const std::string& key, std::any value) override { }
    void* get_state_impl_raw(const std::string& key) override { return nullptr; }
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
    // Will use routine_promise<ComplexRoutine> when implemented
    // using promise_type = complex_promise;

    ProcessingToken get_processing_token() const override
    {
        return ProcessingToken::MULTI_RATE;
    }

    bool requires_clock_sync() const override
    {
        return true; // Complex routines need clock sync for coordination
    }

    // TODO: Implement when multi-domain scheduling is ready
    bool is_active() const override { return false; }
    bool initialize_state(u_int64_t current_context = 0u) override { return false; }
    bool try_resume(u_int64_t current_context) override { return false; }
    bool restart() override { return false; }
    u_int64_t next_execution() const override { return UINT64_MAX; }

protected:
    void set_state_impl(const std::string& key, std::any value) override { }
    void* get_state_impl_raw(const std::string& key) override { return nullptr; }

private:
    ProcessingToken m_primary_token = ProcessingToken::SAMPLE_ACCURATE;
    std::vector<ProcessingToken> m_secondary_tokens;
};
}
