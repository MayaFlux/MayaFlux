#pragma once

#include "config.h"

#include "Clock.hpp"
#include "Routine.hpp"

namespace MayaFlux::Core::Scheduler {

/**
 * @class TaskScheduler
 * @brief Central coordinator for sample-accurate computational processing tasks
 *
 * The TaskScheduler is the heart of the engine's timing system, responsible
 * for managing and executing all computational routines with sample-accurate timing. It
 * maintains a collection of active tasks and advances them in perfect synchronization
 * with the processing timeline.
 *
 * Key responsibilities:
 * 1. Maintaining the master sample clock for the processing engine
 * 2. Managing the lifecycle of computational routines (adding, executing, and removing)
 * 3. Ensuring tasks execute at precisely the right sample positions
 * 4. Advancing all tasks in lockstep with buffer processing
 *
 * The TaskScheduler operates on the processing thread and is designed for real-time
 * performance, with careful attention to avoiding allocations or blocking operations
 * during time-critical processing.
 *
 * This class forms the foundation of the engine's timing infrastructure, enabling
 * complex temporal behaviors to be expressed as simple, sequential coroutines
 * while ensuring they execute with the precision required for time-sensitive applications
 * across multiple domains.
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
    TaskScheduler(unsigned int sample_rate = 48000);

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
     * @brief Converts a time in seconds to a number of samples
     * @param seconds Time duration in seconds
     * @return Equivalent number of samples at the current sample rate
     *
     * This utility method simplifies the conversion between time-based and
     * sample-based measurements, which is frequently needed when scheduling
     * tasks based on real-time values.
     */
    inline u_int64_t seconds_to_samples(double seconds) const
    {
        return static_cast<u_int64_t>(seconds * m_clock.sample_rate());
    }

    /**
     * @brief Gets the sample rate used by the scheduler
     * @return The number of samples per second
     *
     * This method provides access to the sample rate that defines the
     * scheduler's time scale. It's used by tasks to calculate time-based
     * parameters and by the engine to ensure consistent timing.
     */
    inline unsigned int task_sample_rate()
    {
        return m_clock.sample_rate();
    }

    /**
     * @brief Gets a reference to the internal sample clock
     * @return Const reference to the SampleClock
     *
     * This method provides read-only access to the scheduler's internal
     * sample clock, allowing tasks and other components to query the
     * current time without modifying it.
     */
    const SampleClock& get_clock() const { return m_clock; }

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
     * @brief Cancels and removes a task from the scheduler
     * @param task Shared pointer to the task to cancel
     * @return True if the task was found and cancelled, false otherwise
     *
     * This method removes a task from the scheduler, preventing it from
     * executing further. It's used to stop tasks that are no longer needed
     * or to clean up before shutting down the engine.
     */
    bool cancel_task(std::shared_ptr<SoundRoutine> task);

private:
    /**
     * @brief The master sample clock for the processing engine
     *
     * This clock provides the authoritative time reference for all scheduled
     * tasks, ensuring they execute with sample-accurate timing relative to
     * the processing stream.
     */
    SampleClock m_clock;

    /**
     * @brief Collection of active tasks managed by this scheduler
     *
     * This vector contains shared pointers to all computational routines that are
     * currently registered with the scheduler. The scheduler processes
     * these tasks in order during each call to process_sample or process_buffer.
     */
    std::vector<std::shared_ptr<SoundRoutine>> m_tasks;
};

}
