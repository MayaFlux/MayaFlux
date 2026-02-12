#pragma once

namespace MayaFlux::Vruta {
class TaskScheduler;
class SoundRoutine;
}

namespace MayaFlux::Kriya {

/**
 * @class EventChain
 * @brief A sequential chain of timed events with precise temporal control
 *
 * The EventChain class provides a way to schedule a sequence of events to occur
 * at specific time intervals. It's designed for creating temporal sequences of
 * actions with sample-accurate timing, which is essential for deterministic computational flows.
 *
 * This approach is inspired by reactive programming and temporal logic systems
 * where precise sequencing of operations is critical. It allows for creating complex
 * temporal behaviors with a simple, declarative API.
 *
 * Example usage:
 * ```cpp
 * // Create an event chain
 * EventChain chain(*scheduler);
 *
 * // Add events with specific delays
 * chain.then([]() { process_initial_state(); })      // Immediate
 *      .then([]() { transform_data(); }, 0.5)        // After 0.5 seconds
 *      .then([]() { apply_filter(); }, 0.25)         // After another 0.25 seconds
 *      .then([]() { finalize_output(); }, 0.25);     // After another 0.25 seconds
 *
 * // Start the chain
 * chain.start();
 * ```
 *
 * The EventChain is particularly useful for creating precisely timed computational sequences,
 * state transitions, or any series of time-based events that need to occur in a specific
 * order with deterministic timing.
 */
class MAYAFLUX_API EventChain {
public:
    /**
     * @brief Constructs an EventChain using the global scheduler
     *
     * Creates a new EventChain that will use the global scheduler for timing
     * operations. This is convenient for simple cases where only one processing
     * engine is active.
     */
    EventChain();

    /**
     * @brief Constructs an EventChain with an explicit scheduler
     * @param scheduler The TaskScheduler to use for timing
     *
     * Creates a new EventChain that will use the provided scheduler for timing
     * operations. This allows for more control over which scheduler is used,
     * which is useful in contexts where multiple processing engines might exist.
     */
    EventChain(Vruta::TaskScheduler& scheduler);

    /**
     * @brief Adds an event to the chain with a specified delay
     * @param action Function to execute when the event occurs
     * @param delay_seconds Time to wait before executing this event (in seconds)
     * @return Reference to this EventChain for method chaining
     *
     * This method adds an event to the chain, to be executed after the specified
     * delay from the previous event. The first event's delay is measured from
     * when start() is called.
     *
     * The method returns a reference to the EventChain itself, allowing for a
     * fluent, declarative API style.
     */
    EventChain& then(std::function<void()> action, double delay_seconds = 0.F);

    /**
     * @brief Starts executing the event chain
     *
     * This method begins the execution of the event chain, scheduling each event
     * to occur at its specified time. The events are executed in the order they
     * were added, with the specified delays between them.
     *
     * The timing is sample-accurate, ensuring that each event occurs at precisely
     * the right moment in the computational timeline.
     */
    void start();

private:
    /**
     * @brief Structure representing a timed event in the chain
     *
     * This structure encapsulates an action to perform and the delay before
     * performing it, relative to the previous event in the chain.
     */
    struct TimedEvent {
        std::function<void()> action; ///< Function to execute
        double delay_seconds; ///< Delay before execution
    };

    /**
     * @brief Collection of events in this chain
     *
     * This vector contains all the events that have been added to the chain,
     * in the order they will be executed.
     */
    std::vector<TimedEvent> m_events;

    /**
     * @brief Reference to the scheduler that manages timing
     *
     * The scheduler provides the timing infrastructure needed for
     * precise execution of events in the chain.
     */
    Vruta::TaskScheduler& m_Scheduler;

    /**
     * @brief The underlying computational routine that implements the chain
     *
     * This coroutine handles the actual timing and execution of events
     * in the chain. It's created when start() is called.
     */
    std::shared_ptr<Vruta::SoundRoutine> m_routine;
};

}
