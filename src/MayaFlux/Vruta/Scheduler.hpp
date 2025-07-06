#pragma once

#include "Clock.hpp"
#include "Routine.hpp"

namespace MayaFlux::Vruta {

/**
 * @class TaskScheduler
 * @brief Token-based multimodal task scheduling system for unified coroutine processing
 *
 * TaskScheduler serves as the central orchestrator for coroutine scheduling in the MayaFlux engine,
 * implementing a token-based architecture that enables seamless integration of different processing
 * domains while maintaining proven audio scheduling patterns. This design enables true digital-first,
 * data-driven multimedia task scheduling where audio, visual, and custom coroutines can coexist
 * in a unified scheduling system.
 *
 * **Core Architecture:**
 * - **Token-Based Scheduling**: Each processing domain (SAMPLE_ACCURATE, FRAME_ACCURATE, etc.)
 *   has its own dedicated scheduling characteristics and clock synchronization
 * - **Domain-Specific Clocks**: Audio uses SampleClock, video will use FrameClock, etc.
 *   Each domain maintains its own timing reference for precise scheduling
 * - **Proven Processing Patterns**: Maintains the existing successful audio scheduling flow
 *   while extending it to other domains through token-based routing
 * - **Cross-Modal Coordination**: Enables coordination between domains for synchronized effects
 *
 * The scheduler creates appropriate clocks and task lists based on processing tokens,
 * ensuring optimal performance for each domain while enabling cross-domain synchronization.
 */
class TaskScheduler {
public:
    /**
     * @brief Constructs a TaskScheduler with the specified sample rate
     * @param sample_rate The number of samples per second (default: 48000)
     *
     * Creates a new TaskScheduler with an internal SampleClock initialized
     * to the given sample rate. The sample rate determines the relationship
     * between sample counts and real-time durations for all scheduled tasks.
     */
    TaskScheduler(u_int32_t default_sample_rate = 48000, u_int32_t default_frame_rate = 60);

    /**
     * @brief Add a routine to the scheduler based on its processing token
     * @param routine Routine to add (SoundRoutine, GraphicsRoutine, or ComplexRoutine)
     * @param initialize Whether to initialize the routine's state immediately
     *
     * The routine's processing token determines which domain it belongs to and
     * which clock it synchronizes with. Automatically dispatches to the appropriate
     * token-specific task list and clock synchronization.
     */
    void add_task(std::shared_ptr<Routine> routine, bool initialize = false);

    /**
     * @brief Adds a task to the scheduler
     * @param task Shared pointer to the computational routine to schedule
     * @param initialize Whether to initialize the task's state (default: false)
     *
     * Registers a computational routine with the scheduler, making it eligible for
     * execution according to its timing requirements. If initialize is true,
     * the task's state is initialized with the current sample position.
     *
     * The scheduler takes shared ownership of the task, allowing it to be
     * safely referenced from multiple places in the codebase.
     */
    void add_task(std::shared_ptr<SoundRoutine> task, bool initialize = false);

    /**
     * @brief Process all tasks for a specific processing domain
     * @param token Processing domain to advance
     * @param processing_units Number of units to process (samples/frames/etc.)
     *
     * Advances the appropriate clock and processes all tasks that are ready
     * to execute in the specified domain. This is the main entry point for
     * backend-specific processing loops.
     */
    void process_token(ProcessingToken token, u_int64_t processing_units = 1);

    /**
     * @brief Process all active domains
     *
     * Processes all domains that have active tasks, advancing each domain's
     * clock by its default processing unit size. Useful for unified processing
     * in applications that need to advance all domains simultaneously.
     */
    void process_all_tokens();

    /**
     * @brief Register a custom processor for a specific token domain
     * @param token Processing domain to handle
     * @param processor Function that receives tasks and processing units for custom scheduling
     *
     * Allows registering domain-specific scheduling algorithms. For example,
     * a graphics backend might register a processor that batches frame-accurate
     * tasks for optimal GPU utilization.
     */
    void register_token_processor(
        ProcessingToken token,
        std::function<void(const std::vector<std::shared_ptr<Routine>>&, u_int64_t)> processor);

    /**
     * @brief Processes a single sample of time
     *
     * Advances the sample clock by one sample and executes any tasks that
     * are scheduled for the current sample position. This is the core method
     * that drives the execution of all scheduled tasks.
     *
     * This method is typically called once per sample in sample-by-sample
     * processing scenarios, or indirectly via process_buffer for buffer-based
     * processing.
     */
    void process_sample();

    /**
     * @brief Processes a block of samples
     * @param buffer_size Number of samples in the buffer
     *
     * Advances the sample clock by buffer_size samples and executes any tasks
     * that are scheduled during this time period. This method is optimized for
     * buffer-based processing, which is the common case in most real-time systems.
     *
     * This method is typically called once per processing buffer in the main
     * processing callback.
     */
    void process_buffer(unsigned int buffer_size);

    /**
     * @brief Convert seconds to processing units for a specific domain
     * @param seconds Time in seconds
     * @param token Processing domain (default: audio)
     * @return Number of processing units (samples/frames/etc.)
     */
    u_int64_t seconds_to_units(double seconds, ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

    /**
     * @brief Get current processing units for a domain
     * @param token Processing domain
     * @return Current position in the domain's timeline
     */
    u_int64_t current_units(ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

    /**
     * @brief Get processing rate for a domain
     * @param token Processing domain
     * @return Processing rate (sample rate, frame rate, etc.)
     */
    unsigned int get_rate(ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

    /**
     * @brief Converts a time in seconds to a number of samples
     * @param seconds Time duration in seconds
     * @return Equivalent number of samples at the current sample rate
     *
     * Convenience method that uses the audio domain's sample rate for conversion.
     * For domain-specific conversions, use seconds_to_units() with the appropriate token.
     */
    u_int64_t seconds_to_samples(double seconds) const;

    /**
     * @brief Gets the sample rate used by the audio scheduling domain
     * @return The number of samples per second for audio processing
     *
     * Legacy method for compatibility. Equivalent to get_rate(ProcessingToken::SAMPLE_ACCURATE).
     */
    inline unsigned int task_sample_rate()
    {
        return get_rate(ProcessingToken::SAMPLE_ACCURATE);
    }

    /**
     * @brief Get the audio domain's SampleClock (legacy interface)
     * @return Reference to the audio domain's SampleClock
     *
     * Provides backward compatibility for code that expects direct SampleClock access.
     * New code should use get_clock() or get_typed_clock<SampleClock>().
     */
    inline const SampleClock& get_sample_clock() const
    {
        return get_typed_clock<SampleClock>(ProcessingToken::SAMPLE_ACCURATE);
    }

    /**
     * @brief Gets the primary clock (audio domain for legacy compatibility)
     * @return Const reference to the audio domain's SampleClock
     *
     * Legacy method - returns the audio clock for backward compatibility.
     * New multimodal code should use get_clock(token) for specific domains.
     */
    const SampleClock& get_clock() const { return m_clock; }

    /**
     * @brief Get the clock for a specific processing domain
     * @param token Processing domain
     * @return Reference to the domain's clock, or audio clock if token not found
     */
    const IClock& get_clock(ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

    /**
     * @brief Get a typed clock for a specific processing domain
     * @tparam ClockType Type of clock to retrieve (e.g., SampleClock, FrameClock)
     * @param token Processing domain
     * @return Const reference to the typed clock
     *
     * This method allows retrieving a specific type of clock for the given
     * processing domain, ensuring type safety and correct clock usage.
     */
    template <typename ClockType>
    const ClockType& get_typed_clock(ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const
    {
        return dynamic_cast<const ClockType&>(get_clock(token));
    }

    /**
     * @brief Gets a const reference to the collection of active tasks
     * @return Const reference to the vector of task pointers
     *
     * This method provides read-only access to the scheduler's task collection,
     * which is useful for monitoring and debugging purposes.
     */
    inline const std::vector<std::shared_ptr<SoundRoutine>>& get_tasks() const { return m_tasks; }

    /**
     * @brief Gets a mutable reference to the collection of active tasks
     * @return Mutable reference to the vector of task pointers
     *
     * This method provides read-write access to the scheduler's task collection,
     * which is useful for advanced task management operations.
     */
    inline std::vector<std::shared_ptr<SoundRoutine>>& get_tasks() { return m_tasks; }

    /**
     * @brief Get all tasks for a specific processing domain
     * @param token Processing domain
     * @return Vector of tasks in the domain
     */
    const std::vector<std::shared_ptr<Routine>>& get_tasks(ProcessingToken token) const;

    /**
     * @brief Get all audio tasks (legacy interface)
     * @return Vector of audio tasks
     */
    std::vector<std::shared_ptr<SoundRoutine>> get_audio_tasks() const;

    /**
     * @brief Cancel a specific routine
     * @param routine Routine to cancel
     * @return True if routine was found and cancelled, false otherwise
     */
    bool cancel_task(std::shared_ptr<Routine> routine);

    /**
     * @brief Cancels and removes a task from the scheduler
     * @param task Shared pointer to the task to cancel
     * @return True if the task was found and cancelled, false otherwise
     *
     * This method removes a task from the scheduler, preventing it from
     * executing further. It's used to stop tasks that are no longer needed
     * or to clean up before shutting down the engine.
     */
    bool cancel_task(std::shared_ptr<SoundRoutine> task);

    /**
     * @brief Generates a unique task ID for new tasks
     * @return A unique task ID
     */
    const u_int64_t get_next_task_id() const;

    /**
     * @brief Check if a processing domain has any active tasks
     * @param token Processing domain to check
     * @return True if domain has active tasks
     */
    bool has_active_tasks(ProcessingToken token) const;

private:
    /**
     * @brief Initialize a processing domain if it doesn't exist
     * @param token Processing domain to initialize
     * @param rate Processing rate for the domain
     */
    void ensure_token_domain(ProcessingToken token, unsigned int rate = 0);

    /**
     * @brief Get the default rate for a processing token
     * @param token Processing domain
     * @return Default rate for the domain
     */
    unsigned int get_default_rate(ProcessingToken token) const;

    /**
     * @brief Process tasks in a specific domain with default algorithm
     * @param token Processing domain
     * @param processing_units Number of units to process
     */
    void process_token_default(ProcessingToken token, u_int64_t processing_units);

    /**
     * @brief Clean up completed tasks in a domain
     * @param token Processing domain to clean
     */
    void cleanup_completed_tasks(ProcessingToken token);

    /**
     * @brief Initialize a routine's state for a specific domain
     * @param routine Routine to initialize
     * @param token Processing domain
     */
    bool initialize_routine_state(std::shared_ptr<Routine> routine, ProcessingToken token);

    /**
     * @brief Storage for tasks organized by processing token
     *
     * Each processing domain maintains its own task list for efficient
     * scheduling and clock synchronization.
     */
    std::unordered_map<ProcessingToken, std::vector<std::shared_ptr<Routine>>> m_token_tasks;

    /**
     * @brief Clock instances for each processing domain
     *
     * Each domain maintains its own clock for precise timing.
     * Audio uses SampleClock, graphics will use FrameClock, etc.
     */
    std::unordered_map<ProcessingToken, std::unique_ptr<IClock>> m_token_clocks;

    /**
     * @brief Custom processors for specific domains
     *
     * Allows registering domain-specific scheduling algorithms that can
     * optimize task execution for particular backends or use cases.
     */
    std::unordered_map<ProcessingToken, std::function<void(const std::vector<std::shared_ptr<Routine>>&, u_int64_t)>> m_token_processors;

    /**
     * @brief Default processing rates for each domain
     *
     * Stores the rate (samples/sec, frames/sec, etc.) for each domain
     * to enable proper time conversions and clock initialization.
     */
    std::unordered_map<ProcessingToken, unsigned int> m_token_rates;

    /**
     * @brief Mutex for thread-safe access to scheduler state
     */
    mutable std::mutex m_scheduler_mutex;

    /**
     * @brief Task ID counter for unique identification
     */
    mutable std::atomic<u_int64_t> m_next_task_id { 1 };

    /**
     * @brief The master sample clock for the processing engine
     *
     * This clock provides the authoritative time reference for all scheduled
     * tasks, ensuring they execute with sample-accurate timing relative to
     * the processing stream.
     */
    SampleClock m_clock;

    /**
     * @brief Legacy audio domain clock for backward compatibility
     *
     * Maintained for existing code that expects direct SampleClock access.
     * The multimodal system stores this same clock in m_token_clocks for
     * the SAMPLE_ACCURATE token.
     */
    SampleClock m_audio_clock;

    /**
     * @brief Legacy audio tasks collection for backward compatibility
     *
     * Maintained for existing code. The multimodal system stores audio tasks
     * in m_token_tasks under the SAMPLE_ACCURATE token.
     */
    std::vector<std::shared_ptr<SoundRoutine>> m_tasks;
};

}
