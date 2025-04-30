#pragma once

#include "Tasks.hpp"

namespace MayaFlux::Nodes {
class Node;
class NodeGraphManager;
}

namespace MayaFlux::Kriya {

/**
 * @class Timer
 * @brief High-level utility for scheduling one-shot timed callbacks
 *
 * The Timer class provides a convenient way to schedule a function to execute
 * after a specified delay. It wraps the lower-level computational routine system in a
 * simple interface that's easier to use for common timing scenarios.
 *
 * Unlike traditional timers which can drift due to system load, Timer uses
 * the engine's sample-accurate clock to ensure precise, deterministic timing
 * that's perfectly synchronized with the processing pipeline.
 *
 * Example usage:
 * ```cpp
 * // Create a timer
 * Timer timer(*scheduler);
 *
 * // Schedule a callback to execute after 2 seconds
 * timer.schedule(2.0, []() {
 *     std::cout << "Two seconds have passed!" << std::endl;
 * });
 * ```
 *
 * Only one callback can be scheduled at a time; scheduling a new callback
 * cancels any previously scheduled callback.
 */
class Timer {
public:
    /**
     * @brief Constructs a Timer with the specified scheduler
     * @param scheduler The TaskScheduler that will manage this timer
     *
     * Creates a new Timer that will use the provided scheduler for
     * timing operations. The scheduler provides the sample clock and
     * task management infrastructure needed for precise timing.
     */
    Timer(Core::Scheduler::TaskScheduler& scheduler);
    ~Timer() = default;

    /**
     * @brief Schedules a callback to execute after a delay
     * @param delay_seconds Time to wait before executing the callback (in seconds)
     * @param callback Function to execute after the delay
     *
     * This method schedules the provided callback to execute after exactly
     * the specified delay. If a callback is already scheduled, it is cancelled
     * and replaced with the new one.
     *
     * The timing is sample-accurate, ensuring that the callback executes at
     * precisely the right moment in the processing timeline.
     */
    void schedule(double delay_seconds, std::function<void()> callback);

    /**
     * @brief Cancels any scheduled callback
     *
     * This method cancels any currently scheduled callback, preventing it
     * from executing. If no callback is scheduled, this method has no effect.
     */
    void cancel();

    /**
     * @brief Checks if a callback is currently scheduled
     * @return True if a callback is scheduled, false otherwise
     *
     * This method returns true if the timer has an active callback scheduled
     * to execute in the future, and false otherwise.
     */
    inline bool is_active() const { return m_active; }

private:
    /**
     * @brief Reference to the scheduler that manages this timer
     *
     * The scheduler provides the timing infrastructure needed to
     * execute callbacks at precise sample positions.
     */
    Core::Scheduler::TaskScheduler& m_Scheduler;

    /**
     * @brief The underlying computational routine that implements the timer
     *
     * This coroutine handles the actual timing and callback execution.
     * It's created when schedule() is called and destroyed when the
     * callback executes or cancel() is called.
     */
    std::shared_ptr<Core::Scheduler::SoundRoutine> m_routine;

    /**
     * @brief Flag indicating whether a callback is currently scheduled
     *
     * This flag is set to true when schedule() is called and reset to
     * false when the callback executes or cancel() is called.
     */
    bool m_active;

    /**
     * @brief The callback function to execute when the timer fires
     *
     * This function is stored when schedule() is called and executed
     * when the timer fires. It's cleared when cancel() is called.
     */
    std::function<void()> m_callback;
};

/**
 * @class TimedAction
 * @brief Utility for executing actions with a start and end function over a duration
 *
 * The TimedAction class provides a way to execute a pair of functions with a
 * specified time interval between them. This is useful for operations that need
 * to start and then automatically stop after a duration, such as activating a process,
 * applying a transformation, or implementing any time-bounded state change.
 *
 * Example usage:
 * ```cpp
 * // Create a timed action
 * TimedAction action(*scheduler);
 *
 * // Execute an action that lasts for 3 seconds
 * action.execute(
 *     []() { std::cout << "Starting action" << std::endl; },
 *     []() { std::cout << "Ending action" << std::endl; },
 *     3.0
 * );
 * ```
 *
 * Only one action can be active at a time; executing a new action cancels
 * any previously active action.
 */
class TimedAction {
public:
    /**
     * @brief Constructs a TimedAction with the specified scheduler
     * @param scheduler The TaskScheduler that will manage this action
     *
     * Creates a new TimedAction that will use the provided scheduler for
     * timing operations. The scheduler provides the sample clock and
     * task management infrastructure needed for precise timing.
     */
    TimedAction(Core::Scheduler::TaskScheduler& scheduler);
    ~TimedAction() = default;

    /**
     * @brief Executes a pair of functions with a time interval between them
     * @param start_func Function to execute immediately
     * @param end_func Function to execute after the duration
     * @param duration_seconds Time between start_func and end_func (in seconds)
     *
     * This method executes start_func immediately, then schedules end_func
     * to execute after the specified duration. This creates a timed action
     * that starts now and automatically ends after the duration.
     *
     * If an action is already in progress, it is cancelled before starting
     * the new one.
     */
    void execute(std::function<void()> start_func, std::function<void()> end_func, double duration_seconds);

    /**
     * @brief Cancels any active action
     *
     * This method cancels any currently active action, preventing the end
     * function from executing. If no action is active, this method has no effect.
     */
    void cancel();

    /**
     * @brief Checks if an action is currently in progress
     * @return True if an action is in progress, false otherwise
     *
     * This method returns true if an action has been started but not yet
     * completed or cancelled, and false otherwise.
     */
    bool is_pending() const;

private:
    /**
     * @brief Reference to the scheduler that manages this action
     *
     * The scheduler provides the timing infrastructure needed to
     * execute the end function at the right time.
     */
    Core::Scheduler::TaskScheduler& m_Scheduler;

    /**
     * @brief The timer used to schedule the end function
     *
     * This timer is used to execute the end function after the
     * specified duration has elapsed.
     */
    Timer m_timer;
};

/**
 * @class NodeTimer
 * @brief Specialized timer for controlling computational nodes with precise timing
 *
 * The NodeTimer class provides a convenient way to activate processing nodes for
 * specific durations with sample-accurate timing. It handles the details
 * of connecting and disconnecting nodes from the processing graph at precisely
 * the right moments.
 *
 * This is particularly useful for activating temporary processes, applying time-limited
 * transformations, or creating precisely timed events in any computational domain.
 *
 * Example usage:
 * ```cpp
 * // Create a node timer
 * NodeTimer timer(*scheduler, *graph_manager);
 *
 * // Activate a processing node for exactly 2 seconds
 * timer.play_for(process_node, 2.0);
 * ```
 *
 * Only one node can be active at a time; activating a new node cancels
 * any previously active node.
 */
class NodeTimer {
public:
    /**
     * @brief Constructs a NodeTimer with the specified scheduler
     * @param scheduler The TaskScheduler that will manage this timer
     *
     * Creates a new NodeTimer that will use the provided scheduler for
     * timing operations. The scheduler provides the sample clock and
     * task management infrastructure needed for precise timing.
     */
    NodeTimer(Core::Scheduler::TaskScheduler& scheduler);

    /**
     * @brief Constructs a NodeTimer with the specified scheduler and graph manager
     * @param scheduler The TaskScheduler that will manage this timer
     * @param graph_manager The NodeGraphManager that will manage the processing nodes
     *
     * Creates a new NodeTimer that will use the provided scheduler and
     * graph manager for timing and node management operations.
     */
    NodeTimer(Core::Scheduler::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager);
    ~NodeTimer() = default;

    /**
     * @brief Activates a processing node for a specific duration
     * @param node The processing node to activate
     * @param duration_seconds How long to keep the node active (in seconds)
     * @param channel The output channel to connect the node to (default: 0)
     *
     * This method connects the specified node to the output channel,
     * activates it for the specified duration, then automatically disconnects it.
     * The timing is sample-accurate, ensuring that the node remains active for
     * exactly the right duration.
     *
     * If a node is already active, it is deactivated before starting the new one.
     */
    void play_for(std::shared_ptr<Nodes::Node> node, double duration_seconds, unsigned int channel = 0);

    /**
     * @brief Activates a processing node with custom setup and cleanup functions
     * @param node The processing node to activate
     * @param setup_func Function to call before activating the node
     * @param cleanup_func Function to call after deactivating the node
     * @param duration_seconds How long to keep the node active (in seconds)
     * @param channel The output channel to connect the node to (default: 0)
     *
     * This method provides more control over the node activation process by
     * allowing custom setup and cleanup functions. The setup function is
     * called before connecting the node, and the cleanup function is called
     * after disconnecting it.
     *
     * This is useful for more complex scenarios where additional setup or
     * cleanup steps are needed, such as configuring node parameters or
     * managing resources.
     *
     * If a node is already active, it is deactivated before starting the new one.
     */
    void play_with_processing(std::shared_ptr<Nodes::Node> node,
        std::function<void(std::shared_ptr<Nodes::Node>)> setup_func,
        std::function<void(std::shared_ptr<Nodes::Node>)> cleanup_func,
        double duration_seconds,
        unsigned int channel = 0);

    /**
     * @brief Cancels any currently active node
     *
     * This method deactivates any currently active node, disconnecting it from
     * the output channel. If no node is active, this method has no effect.
     */
    void cancel();

    /**
     * @brief Checks if a node is currently active
     * @return True if a node is active, false otherwise
     *
     * This method returns true if the timer has an active node that's
     * currently processing, and false otherwise.
     */
    inline bool is_active() const { return m_timer.is_active(); }

private:
    /**
     * @brief Reference to the scheduler that manages this timer
     *
     * The scheduler provides the timing infrastructure needed for
     * precise control of node activation durations.
     */
    Core::Scheduler::TaskScheduler& m_scheduler;

    /**
     * @brief Reference to the graph manager that manages processing nodes
     *
     * The graph manager provides the infrastructure for connecting
     * and disconnecting nodes from the processing graph.
     */
    Nodes::NodeGraphManager& m_node_graph_manager;

    /**
     * @brief The timer used to schedule node disconnection
     *
     * This timer is used to disconnect the node after the
     * specified duration has elapsed.
     */
    Timer m_timer;

    /**
     * @brief The currently active node being played
     *
     * This pointer holds the node that is currently being played.
     * It is set when play_for() or play_with_processing() is called
     * and reset when the node finishes playing or is cancelled.
     */
    std::shared_ptr<Nodes::Node> m_current_node;

    /**
     * @brief The output channel the current node is connected to
     *
     * This variable stores the output channel number that the current
     * node is connected to. It is set when play_for() or play_with_processing()
     * is called and reset when the node finishes playing or is cancelled.
     */
    unsigned int m_current_channel = 0;
};

}
