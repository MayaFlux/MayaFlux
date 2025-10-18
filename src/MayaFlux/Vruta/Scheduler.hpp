#pragma once

#include "Clock.hpp"
#include "Routine.hpp"

namespace MayaFlux::Vruta {

struct TaskEntry {
    std::shared_ptr<Routine> routine;
    std::string name;

    TaskEntry(std::shared_ptr<Routine> r, const std::string& n)
        : routine(std::move(r))
        , name(std::move(n))
    {
    }
};

/** @typedef token_processing_func_t
 *  @brief Function type for processing tasks in a specific token domain
 *
 *  This function type defines the signature for custom processing functions
 *  that can be registered to handle tasks in a specific processing domain.
 *  The function receives a vector of routines and the number of processing
 *  units (samples, frames, etc.) to advance.
 */
using token_processing_func_t = std::function<void(const std::vector<std::shared_ptr<Routine>>&, uint64_t)>;

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
class MAYAFLUX_API TaskScheduler {
public:
    /**
     * @brief Constructs a TaskScheduler with the specified sample rate
     * @param sample_rate The number of samples per second (default: 48000)
     *
     * Creates a new TaskScheduler with an internal SampleClock initialized
     * to the given sample rate. The sample rate determines the relationship
     * between sample counts and real-time durations for all scheduled tasks.
     */
    TaskScheduler(uint32_t default_sample_rate = 48000, uint32_t default_frame_rate = 60);

    /**
     * @brief Add a routine to the scheduler based on its processing token
     * @param routine Routine to add (SoundRoutine, GraphicsRoutine, or ComplexRoutine)
     * @param name Optional name for the routine (for task management)
     * @param initialize Whether to initialize the routine's state immediately
     *
     * The routine's processing token determines which domain it belongs to and
     * which clock it synchronizes with. Automatically dispatches to the appropriate
     * token-specific task list and clock synchronization.
     */
    void add_task(std::shared_ptr<Routine> routine, const std::string& name = "", bool initialize = false);

    /**
     * @brief Get a named task
     * @param name Task name
     * @return Shared pointer to routine or nullptr
     */
    std::shared_ptr<Routine> get_task(const std::string& name) const;

    /**
     * @brief Cancels and removes a task from the scheduler
     * @param task Shared pointer to the task to cancel
     * @return True if the task was found and cancelled, false otherwise
     *
     * This method removes a task from the scheduler, preventing it from
     * executing further. It's used to stop tasks that are no longer needed
     * or to clean up before shutting down the engine.
     */
    bool cancel_task(std::shared_ptr<Routine> task);

    /**
     * @brief Cancel a task by name
     * @param name Task name to cancel
     * @return True if found and cancelled
     */
    bool cancel_task(const std::string& name);

    /**
     * @brief Restart a named task
     * @param name Task name to restart
     * @return True if found and restarted
     */
    bool restart_task(const std::string& name);

    /**
     * @brief Get all tasks for a specific processing domain
     * @param token Processing domain
     * @return Vector of tasks in the domain
     */
    std::vector<std::shared_ptr<Routine>> get_tasks_for_token(ProcessingToken token) const;

    /**
     * @brief Process all tasks for a specific processing domain
     * @param token Processing domain to advance
     * @param processing_units Number of units to process (samples/frames/etc.)
     *
     * Advances the appropriate clock and processes all tasks that are ready
     * to execute in the specified domain. This is the main entry point for
     * backend-specific processing loops.
     */
    void process_token(ProcessingToken token, uint64_t processing_units = 1);

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
    void register_token_processor(ProcessingToken token, token_processing_func_t processor);

    /**
     * @brief Convert seconds to processing units for a specific domain
     * @param seconds Time in seconds
     * @param token Processing domain (default: audio)
     * @return Number of processing units (samples/frames/etc.)
     */
    uint64_t seconds_to_units(double seconds, ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

    /**
     * @brief Get current processing units for a domain
     * @param token Processing domain
     * @return Current position in the domain's timeline
     */
    uint64_t current_units(ProcessingToken token = ProcessingToken::SAMPLE_ACCURATE) const;

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
    uint64_t seconds_to_samples(double seconds) const;

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
     * @brief Update parameters of a named task
     * @tparam Args Parameter types
     * @param name Task name
     * @param args New parameters
     * @return True if task found and updated
     */
    template <typename... Args>
    bool update_task_params(const std::string& name, Args&&... args)
    {
        auto it = find_task_by_name(name);
        if (it != m_tasks.end() && it->routine && it->routine->is_active()) {
            it->routine->update_params(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }

    /**
     * @brief Get task state value by name and key
     * @tparam T State value type
     * @param name Task name
     * @param state_key State key
     * @return Pointer to value or nullptr
     */
    template <typename T>
    T* get_task_state(const std::string& name, const std::string& state_key) const
    {
        auto it = find_task_by_name(name);
        if (it != m_tasks.end() && it->routine && it->routine->is_active()) {
            return it->routine->get_state<T>(state_key);
        }
        return nullptr;
    }

    /**
     * @brief Create value accessor function for named task
     * @tparam T Value type
     * @param name Task name
     * @param state_key State key
     * @return Function returning current value
     */
    template <typename T>
    std::function<T()> create_value_accessor(const std::string& name, const std::string& state_key) const
    {
        return [this, name, state_key]() -> T {
            if (auto value = get_task_state<T>(name, state_key)) {
                return *value;
            }
            return T {};
        };
    }

    /**
     * @brief Generates a unique task ID for new tasks
     * @return A unique task ID
     */
    uint64_t get_next_task_id() const;

    /**
     * @brief Check if a processing domain has any active tasks
     * @param token Processing domain to check
     * @return True if domain has active tasks
     */
    bool has_active_tasks(ProcessingToken token) const;

    /**
     * @brief Get all task names for debugging/inspection
     * @return Vector of all task names
     */
    std::vector<std::string> get_task_names() const;

    /**
     * @brief Pause all active tasks
     */
    void pause_all_tasks();

    /**
     * @brief Resume all previously paused tasks
     */
    void resume_all_tasks();

    /**
     * @brief Terminate and clear all tasks
     */
    void terminate_all_tasks();

    /** @brief Get the task cleanup threshold
     *
     * This value determines how many processing units must pass before
     * the scheduler cleans up completed tasks.
     */
    inline uint32_t get_cleanup_threshold() const { return m_cleanup_threshold; }

    /** @brief Set the task cleanup threshold
     * @param threshold New threshold value
     *
     * This value determines how many processing units must pass before
     * the scheduler cleans up completed tasks. Lower values increase
     * cleanup frequency, while higher values reduce overhead.
     */
    inline void set_cleanup_threshold(uint32_t threshold) { m_cleanup_threshold = threshold; }

private:
    /**
     * @brief Generate automatic name for a routine based on its type
     * @param routine The routine to name
     * @return Generated name
     */
    std::string auto_generate_name(std::shared_ptr<Routine> routine) const;

    /**
     * @brief Find task entry by name
     * @param name Task name to find
     * @return Iterator to task entry or end()
     */
    std::vector<TaskEntry>::iterator find_task_by_name(const std::string& name);

    /**
     * @brief Find task entry by name (const version)
     * @param name Task name to find
     * @return Const iterator to task entry or end()
     */
    std::vector<TaskEntry>::const_iterator find_task_by_name(const std::string& name) const;

    /**
     * @brief Find task entry by routine pointer
     * @param routine Routine to find
     * @return Iterator to task entry or end()
     */
    std::vector<TaskEntry>::iterator find_task_by_routine(std::shared_ptr<Routine> routine);

    /**
     * @brief Initialize a processing domain if it doesn't exist
     * @param token Processing domain to initialize
     * @param rate Processing rate for the domain
     */
    void ensure_domain(ProcessingToken token, unsigned int rate = 0);

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
    void process_default(ProcessingToken token, uint64_t processing_units);

    /**
     * @brief Clean up completed tasks in a domain
     */
    void cleanup_completed_tasks();

    /**
     * @brief Initialize a routine's state for a specific domain
     * @param routine Routine to initialize
     * @param token Processing domain
     */
    bool initialize_routine_state(std::shared_ptr<Routine> routine, ProcessingToken token);

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
    std::unordered_map<ProcessingToken, token_processing_func_t> m_token_processors;

    /**
     * @brief Default processing rates for each domain
     *
     * Stores the rate (samples/sec, frames/sec, etc.) for each domain
     * to enable proper time conversions and clock initialization.
     */
    std::unordered_map<ProcessingToken, unsigned int> m_token_rates;

    /**
     * @brief Task ID counter for unique identification
     */
    mutable std::atomic<uint64_t> m_next_task_id { 1 };

    std::vector<TaskEntry> m_tasks;

    /**
     * @brief The master sample clock for the processing engine
     *
     * This clock provides the authoritative time reference for all scheduled
     * tasks, ensuring they execute with sample-accurate timing relative to
     * the processing stream.
     */
    SampleClock m_clock;

    /** @brief Threshold for task cleanup
     *
     * This value determines how many processing units must pass before
     * the scheduler cleans up completed tasks. It helps manage memory
     * and performance by removing inactive tasks periodically.
     */
    uint32_t m_cleanup_threshold;
};

}
