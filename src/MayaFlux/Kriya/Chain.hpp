#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"

#include "Awaiters/BroadcastAwaiter.hpp"

namespace MayaFlux::Vruta {
class TaskScheduler;
class Routine;
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
 * EventChain chain(scheduler);
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
     * @brief Constructs an EventChain with an explicit scheduler
     * @param scheduler The TaskScheduler to use for timing
     * @param name Optional name for the event chain (useful for debugging)
     * @param token SAMPLE_ACCURATE runs the chain on the sample clock; any other token runs it on the frame clock
     *
     * Creates a new EventChain that adds its routine to the provided scheduler.
     * Delays convert at the registered sample or frame rate.
     * The chain keeps the scheduler alive for as long as it exists.
     */
    EventChain(const std::shared_ptr<Vruta::TaskScheduler>& scheduler, std::string name = "", Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE);

    /** @deprecated Pass the scheduler as a shared_ptr. The chain does not own it. */
    [[deprecated("pass the scheduler as a std::shared_ptr")]]
    EventChain(Vruta::TaskScheduler& scheduler, std::string name = "", Vruta::ProcessingToken token = Vruta::ProcessingToken::SAMPLE_ACCURATE)
        : EventChain(
              std::shared_ptr<Vruta::TaskScheduler>(&scheduler, [](Vruta::TaskScheduler*) { }),
              std::move(name), token)
    {
    }

    /**
     * @brief Adds an event to the chain with a specified delay
     * @param action Function to execute when the event occurs
     * @param delay_seconds Time to wait before executing this event (in seconds)
     * @return Reference to this EventChain for method chaining
     *
     * The delay is measured from the previous event; the first event's delay is
     * measured from when start() is called.
     */
    EventChain& then(std::function<void()> action, double delay_seconds = 0.F);

    /**
     * @brief Adds an event that runs when a BroadcastSource next signals
     * @param source Source to watch; the chain keeps it alive
     * @param action Function that takes the signalled value
     * @return Reference to this EventChain for method chaining
     *
     * The chain checks the source on its own clock, so the action runs where the
     * chain runs, not on the thread that signals. A source holds one pending
     * value, so signals that arrive between two checks collapse into the last.
     */
    template <typename T>
    EventChain& on(std::shared_ptr<Vruta::BroadcastSource<T>> source, std::function<void(const T&)> action)
    {
        return poll(
            [source = std::move(source), action = std::move(action)]() {
                auto awaiter = source->next();
                if (!awaiter.await_ready()) {
                    return false;
                }
                action(awaiter.await_resume());
                return true;
            });
    }

    /**
     * @brief Adds an event that runs once a condition holds
     * @param condition Checked once per unit of the clock until it returns true
     * @param action Function to execute when it does
     * @return Reference to this EventChain for method chaining
     */
    EventChain& when(std::function<bool()> condition, std::function<void()> action);

    /**
     * @brief Repeat the last event N times
     * @param count Number of times to repeat
     * @return Reference to this EventChain for method chaining
     */
    EventChain& repeat(size_t count);

    /**
     * @brief Repeat entire chain N times
     * @param count Number of times to repeat the entire sequence
     * @return Reference to this EventChain for method chaining
     */
    EventChain& times(size_t count);

    /**
     * @brief Add a wait/delay without an action
     * @param delay_seconds Time to wait in seconds
     * @return Reference to this EventChain for method chaining
     */
    EventChain& wait(double delay_seconds);

    /**
     * @brief Convenience method for repeating an action at regular intervals
     * @param interval_seconds Time between each execution
     * @param action Function to execute
     * @return Reference to this EventChain for method chaining
     */
    EventChain& every(double interval_seconds, std::function<void()> action);

    /**
     * @brief Adds a step that runs several lanes side by side and waits for all of them
     * @param lanes One builder per lane; each fills a chain that shares this chain's scheduler and clock
     * @return Reference to this EventChain for method chaining
     *
     * The lanes start together when the chain reaches the step, each with its
     * own steps and timing, and the chain continues once every lane has
     * finished. Cancelling the chain cancels its lanes. A lane may add lanes
     * of its own. The step counts as two in event_count(): the start and the wait.
     *
     * ```cpp
     * chain.then(open)
     *      .parallel({
     *          [](EventChain& lane) { lane.then(a, 0.2).wait(0.1); },
     *          [](EventChain& lane) { lane.every(0.05, b).times(4); } })
     *      .then(close);
     * ```
     */
    EventChain& parallel(std::vector<std::function<void(EventChain&)>> lanes);

    /**
     * @brief Adds a step that starts several lanes and continues at once
     * @param lanes One builder per lane, as for parallel()
     * @return Reference to this EventChain for method chaining
     *
     * The lanes run on their own while the chain moves on to its next step. They
     * keep running after the chain ends; cancel() cancels them with it.
     */
    EventChain& fork(std::vector<std::function<void(EventChain&)>> lanes);

    /**
     * @brief Repeat the whole chain without end
     * @return Reference to this EventChain for method chaining
     *
     * The chain waits one unit between passes, so a pass of zero delays does not
     * hold the scheduler. Replaces any count set by times().
     */
    EventChain& forever();

    /**
     * @brief Repeat the whole chain until a condition holds
     * @param condition Checked after every pass; the chain stops when it returns true
     * @return Reference to this EventChain for method chaining
     */
    EventChain& until(std::function<bool()> condition);

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

    /**
     * @brief Cancels the event chain if it's currently executing
     *
     * Terminates the underlying coroutine, preventing any remaining
     * events from executing. Safe to call even if chain has completed.
     */
    void cancel();

    /**
     * @brief Checks if the event chain is currently active
     * @return True if chain is executing, false if completed or not started
     */
    [[nodiscard]] bool is_active() const;

    /**
     * @brief Sets a callback to execute when the chain stops executing
     * @param callback Function to call after chain completes or is cancelled
     *
     * The callback fires once, on the thread that stops the chain, regardless of how it stops:
     * - After final event completes normally
     * - After cancel() is called
     * - When a later start() under the same name replaces the chain
     * - When the scheduler terminates all tasks
     *
     * Use this for cleanup that must happen regardless of completion reason.
     * For actions that should only run on successful completion, use .then()
     */
    EventChain& on_complete(std::function<void()> callback);

    /**
     * @brief Gets the name of the event chain
     * @return Name of the event chain
     *
     * The name can be used for debugging or management purposes, especially when
     * multiple chains are active. If no name was set, this will return an empty string.
     */
    [[nodiscard]] const std::string& name() const { return m_name; }

    /**
     * @brief Gets the number of events in the chain
     * @return Number of events
     */
    [[nodiscard]] size_t event_count() const { return m_events.size(); }

    /**
     * @brief Gets the repeat count for the entire chain
     * @return Number of times the chain will repeat
     */
    [[nodiscard]] size_t repeat_count() const { return m_repeat_count; }

private:
    /**
     * @brief One step of the chain
     *
     * A step with a poll waits for it to return true and runs nothing else;
     * any other step waits its delay and then runs its action.
     */
    struct TimedEvent {
        std::function<void()> action; ///< Function to execute
        double delay_seconds; ///< Delay before execution
        std::function<bool()> poll; ///< Checked each unit until it returns true
    };

    EventChain& poll(std::function<bool()> ready);

    std::shared_ptr<std::vector<std::shared_ptr<EventChain>>> start_lanes(std::vector<std::function<void(EventChain&)>> lanes);

    /**
     * @brief Collection of events in this chain
     *
     * This vector contains all the events that have been added to the chain,
     * in the order they will be executed.
     */
    std::vector<TimedEvent> m_events;

    /**
     * @brief The scheduler that manages timing
     *
     * The scheduler provides the timing infrastructure needed for
     * precise execution of events in the chain.
     */
    std::shared_ptr<Vruta::TaskScheduler> m_Scheduler;

    /**
     * @brief Selects the clock the chain runs on, sample or frame
     */
    Vruta::ProcessingToken m_token;

    /**
     * @brief The underlying computational routine that implements the chain
     *
     * This coroutine handles the actual timing and execution of events
     * in the chain. It's created when start() is called.
     */
    std::shared_ptr<Vruta::Routine> m_routine;

    /**
     * @brief Optional callback to execute when the chain completes
     *
     * This function is called after the final event in the chain has executed.
     * It can be used for cleanup, triggering subsequent actions, or any other
     * behavior that should occur after the chain finishes.
     */
    std::function<void()> m_on_complete;

    /**
     * @brief Optional name for the event chain
     *
     * This name can be used for debugging or management purposes, especially when
     * multiple chains are active. If no name is provided, it will be an empty string.
     */
    std::string m_name;

    size_t m_repeat_count { 1 }; ///< Number of times to repeat the entire chain

    bool m_endless { false }; ///< Repeat the entire chain without a count

    std::function<bool()> m_stop_when; ///< Stops the chain after a pass when it returns true

    std::vector<std::shared_ptr<EventChain>> m_lanes; ///< Lanes added by parallel(), cancelled with the chain
};

}
