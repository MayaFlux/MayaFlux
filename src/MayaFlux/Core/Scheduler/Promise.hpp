#pragma once

#include "config.h"

#include <coroutine>

namespace MayaFlux::Core::Scheduler {

class SoundRoutine;

/**
 * @struct promise_type
 * @brief Coroutine promise type for audio processing tasks with sample-accurate timing
 *
 * The promise_type is a crucial component of C++20 coroutines that defines the
 * behavior of SoundRoutine coroutines. It serves as the control interface between
 * the coroutine machinery and the audio engine, managing:
 *
 * 1. Coroutine lifecycle (creation, suspension, resumption, and destruction)
 * 2. Timing information for sample-accurate scheduling
 * 3. State storage for persistent data between suspensions
 * 4. Control flags for execution behavior
 *
 * In the coroutine model, the promise object is created first when a coroutine
 * function is called. It then creates and returns the SoundRoutine object that
 * the caller receives. The promise remains associated with the coroutine frame
 * throughout its lifetime, while the SoundRoutine provides the external interface
 * to manipulate the coroutine.
 *
 * This separation of concerns allows the audio engine to schedule and manage
 * coroutines efficiently while providing a clean API for audio processing code.
 */
struct promise_type {
    /**
     * @brief Creates the SoundRoutine object returned to the caller
     * @return A new SoundRoutine that wraps this promise
     *
     * This method is called by the compiler-generated code when a coroutine
     * function is invoked. It creates the SoundRoutine object that will be
     * returned to the caller and associates it with this promise.
     */
    SoundRoutine get_return_object();

    /**
     * @brief Determines whether the coroutine suspends immediately upon creation
     * @return A suspend never awaitable, meaning the coroutine begins execution immediately
     *
     * By returning std::suspend_never, this method indicates that the coroutine
     * should start executing as soon as it's created, rather than waiting for
     * an explicit resume call.
     */
    std::suspend_never initial_suspend() { return {}; }

    /**
     * @brief Determines whether the coroutine suspends before destruction
     * @return A suspend always awaitable, meaning the coroutine suspends before completing
     *
     * By returning std::suspend_always, this method ensures that the coroutine
     * frame isn't destroyed immediately when the coroutine completes. This gives
     * the scheduler a chance to observe the completion and perform cleanup.
     */
    std::suspend_always final_suspend() noexcept { return {}; }

    /**
     * @brief Handles the coroutine's void return
     *
     * This method is called when the coroutine executes a co_return statement
     * without a value, or reaches the end of its function body.
     */
    void return_void() { }

    /**
     * @brief Handles exceptions thrown from within the coroutine
     *
     * This method is called if an unhandled exception escapes from the coroutine.
     * The current implementation terminates the program, as exceptions in audio
     * processing code are generally considered fatal errors.
     */
    void unhandled_exception() { std::terminate(); }

    /**
     * @brief The sample position when this coroutine should next execute
     *
     * This is the core timing mechanism for sample-accurate scheduling.
     * When a coroutine co_awaits a SampleDelay, this value is updated to
     * indicate when the coroutine should be resumed next.
     */
    u_int64_t next_sample = 0;

    /**
     * @brief Flag indicating whether the coroutine should be automatically resumed
     *
     * When true, the scheduler will automatically resume the coroutine when
     * the current sample position reaches next_sample. When false, the coroutine
     * must be manually resumed by calling SoundRoutine::try_resume.
     */
    bool auto_resume = true;

    /**
     * @brief Flag indicating whether the coroutine should be terminated
     *
     * When set to true, the scheduler will destroy the coroutine rather than
     * resuming it, even if it hasn't completed naturally. This allows for
     * early termination of long-running coroutines.
     */
    bool should_terminate = false;

    /**
     * @brief Dictionary for storing arbitrary state data
     *
     * This map allows the coroutine to store and retrieve named values of any type.
     * It serves multiple purposes:
     * 1. Persistent storage between suspensions
     * 2. Communication channel between the coroutine and external code
     * 3. Parameter storage for configurable behaviors
     *
     * The use of std::any allows for type-safe heterogeneous storage without
     * requiring the promise type to be templated.
     */
    std::unordered_map<std::string, std::any> state;

    /**
     * @brief Stores a value in the state dictionary
     * @param key Name of the state value
     * @param value Value to store
     *
     * This method provides a type-safe way to store values of any type in the
     * state dictionary. The value is wrapped in std::any for type erasure.
     */
    template <typename T>
    inline void set_state(const std::string& key, T value)
    {
        state[key] = std::make_any<T>(std::move(value));
    }

    /**
     * @brief Retrieves a value from the state dictionary
     * @param key Name of the state value to retrieve
     * @return Pointer to the stored value, or nullptr if not found or type mismatch
     *
     * This method provides a type-safe way to retrieve values from the state
     * dictionary. It returns a pointer to the stored value if it exists and
     * has the requested type, or nullptr otherwise.
     */
    template <typename T>
    inline T* get_state(const std::string& key)
    {
        auto it = state.find(key);
        if (it != state.end()) {
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }
};
}
