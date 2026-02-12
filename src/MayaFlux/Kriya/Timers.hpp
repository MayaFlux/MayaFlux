#pragma once

#include "MayaFlux/Core/ProcessingTokens.hpp"
#include "Tasks.hpp"

namespace MayaFlux::Nodes {
class Node;
class NodeGraphManager;
namespace Network {
    class NodeNetwork;
}
}

namespace MayaFlux::Buffers {
class Buffer;
class BufferManager;
}

namespace MayaFlux::Kriya {

struct MAYAFLUX_API TimeSpec {
    double seconds;
    std::optional<std::vector<uint32_t>> channels;

    TimeSpec(double s)
        : seconds(s)
    {
    }
    TimeSpec(double s, std::vector<uint32_t> ch)
        : seconds(s)
        , channels(ch)
    {
    }
    TimeSpec(double s, uint32_t ch)
        : seconds(s)
        , channels(std::vector<uint32_t> { ch })
    {
    }
};

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
class MAYAFLUX_API Timer {
public:
    /**
     * @brief Constructs a Timer with the specified scheduler
     * @param scheduler The TaskScheduler that will manage this timer
     *
     * Creates a new Timer that will use the provided scheduler for
     * timing operations. The scheduler provides the sample clock and
     * task management infrastructure needed for precise timing.
     */
    Timer(Vruta::TaskScheduler& scheduler);
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
    [[nodiscard]] inline bool is_active() const { return m_active; }

private:
    /**
     * @brief Reference to the scheduler that manages this timer
     *
     * The scheduler provides the timing infrastructure needed to
     * execute callbacks at precise sample positions.
     */
    Vruta::TaskScheduler& m_Scheduler;

    /**
     * @brief The underlying computational routine that implements the timer
     *
     * This coroutine handles the actual timing and callback execution.
     * It's created when schedule() is called and destroyed when the
     * callback executes or cancel() is called.
     */
    std::shared_ptr<Vruta::SoundRoutine> m_routine;

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
class MAYAFLUX_API TimedAction {
public:
    /**
     * @brief Constructs a TimedAction with the specified scheduler
     * @param scheduler The TaskScheduler that will manage this action
     *
     * Creates a new TimedAction that will use the provided scheduler for
     * timing operations. The scheduler provides the sample clock and
     * task management infrastructure needed for precise timing.
     */
    TimedAction(Vruta::TaskScheduler& scheduler);
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
    void execute(const std::function<void()>& start_func, const std::function<void()>& end_func, double duration_seconds);

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
    [[nodiscard]] bool is_pending() const;

private:
    /**
     * @brief Reference to the scheduler that manages this action
     *
     * The scheduler provides the timing infrastructure needed to
     * execute the end function at the right time.
     */
    Vruta::TaskScheduler& m_Scheduler;

    /**
     * @brief The timer used to schedule the end function
     *
     * This timer is used to execute the end function after the
     * specified duration has elapsed.
     */
    Timer m_timer;
};

/**
 * @class TemporalActivation
 * @brief Specialized timer for controlling computational nodes, buffers, and networks
 *
 * The TemporalActivation class provides a high-level interface for activating
 * nodes, buffers, or entire networks for a specified duration. It manages the
 * lifecycle of the activated entity, ensuring that it is properly connected to
 * the processing graph when activated and cleanly disconnected when the timer expires.
 *
 * Usage example:
 * ```cpp
 * // Create a temporal activation
 * TemporalActivation activation(*scheduler, node_graph_manager, buffer_manager);
 *
 * // Activate a node for 5 seconds on channels 0 and 1
 * activation.activate_node(my_node, my_token, 5.0, {0, 1});
 * ```
 */
class MAYAFLUX_API TemporalActivation {
public:
    /**
     * @brief Constructs a TemporalActivation with the specified scheduler and manager
     * @param scheduler The TaskScheduler that will manage this timer
     * @param graph_manager The NodeGraphManager that will manage the processing nodes
     * @param buffer_manager The BufferManager that will manage any buffers needed for processing
     *
     * Creates a new NodeTimer that will use the provided scheduler and
     * graph manager for timing and node management operations.
     */
    TemporalActivation(
        Vruta::TaskScheduler& scheduler,
        Nodes::NodeGraphManager& graph_manager,
        Buffers::BufferManager& buffer_manager);

    ~TemporalActivation() = default;

    /**
     * @brief Activates a node for a specified duration
     * @param node The node to activate
     * @param token The processing token associated with the node
     * @param duration_seconds The duration to keep the node active (in seconds)
     * @param channels Optional list of output channels to connect the node to (default is all channels)
     *
     * This method activates the specified node by connecting it to the output channels or graphics sync,
     * and starts a timer for the specified duration. When the timer expires, the node is automatically
     * disconnected from the output channels, effectively deactivating it.
     *
     * If another node, network, or buffer is already active, it will be cancelled before activating the new one.
     */
    void activate_node(const std::shared_ptr<Nodes::Node>& node,
        double duration_seconds,
        Nodes::ProcessingToken token = Nodes::ProcessingToken::AUDIO_RATE,
        const std::vector<uint32_t>& channels = {});

    /**
     * @brief Activates a node network for a specified duration
     * @param network The node network to activate
     * @param token The processing token associated with the network
     * @param duration_seconds The duration to keep the network active (in seconds)
     * @param channels Optional list of output channels to connect the network to (default is all channels)
     *
     * This method activates the specified node network by connecting it to the output channels or graphics sync,
     * and starts a timer for the specified duration. When the timer expires, the network is automatically
     * disconnected from the output channels, effectively deactivating it.
     *
     * If another node, network, or buffer is already active, it will be cancelled before activating the new one.
     */
    void activate_network(const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
        double duration_seconds,
        Nodes::ProcessingToken token = Nodes::ProcessingToken::AUDIO_RATE,
        const std::vector<uint32_t>& channels = {});

    /**
     * @brief Activates a buffer for a specified duration
     * @param buffer The buffer to activate
     * @param token The processing token associated with the buffer
     * @param duration_seconds The duration to keep the buffer active (in seconds)
     * @param channel Optional output channel to connect the buffer to (default is 0)
     * This method activates the specified buffer by connecting it to the output channel,
     * and starts a timer for the specified duration. When the timer expires, the buffer is automatically
     * disconnected from the output channel, effectively deactivating it.
     *
     * If another node, network, or buffer is already active, it will be cancelled before activating the new one.
     */
    void activate_buffer(const std::shared_ptr<Buffers::Buffer>& buffer,
        double duration_seconds,
        Buffers::ProcessingToken token = Buffers::ProcessingToken::AUDIO_BACKEND,
        uint32_t channel = 0);

    /**
     * @brief Cancels any currently active node
     *
     * This method deactivates any currently active node, nodenetowrk or buffer, disconnecting it from
     * the output channel. If no node is active, this method has no effect.
     */
    void cancel();

    /**
     * @brief Checks if a node is currently active
     * @return True if a node is active, false otherwise
     *
     * This method returns true if the timer has an active entity that's
     * currently processing, and false otherwise.
     */
    [[nodiscard]] inline bool is_active() const { return m_timer.is_active(); }

private:
    /**
     * @brief Reference to the scheduler that manages this timer
     *
     * The scheduler provides the timing infrastructure needed for
     * precise control of entity activation durations.
     */
    Vruta::TaskScheduler& m_scheduler;

    /**
     * @brief Cleans up the current operation, disconnecting the entity and resetting state
     *
     * This method is called when the timer completes its specified duration.
     * It disconnects the currently active node/nodenetwork/buffer from the
     *  output channels or graphics syc, or active and
     * resets internal state to allow new operations to be started.
     */
    void cleanup_current_operation();

    /**
     * @brief Reference to the graph manager that manages processing nodes
     *
     * The graph manager provides the infrastructure for connecting
     * and disconnecting nodes from the processing graph.
     */
    Nodes::NodeGraphManager& m_node_graph_manager;

    /**
     * @brief Reference to the buffer manager that manages processing buffers
     *
     * The buffer manager provides the infrastructure for managing any buffers
     * needed for processing nodes during their active duration.
     */
    Buffers::BufferManager& m_buffer_manager;

    /**
     * @brief The timer used to schedule processing duration
     *
     * This timer is used to disconnect the entity after the
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
     * @brief The currently active network being played
     *
     * This pointer holds the network that is currently being played.
     * It is set when play_for() or play_with_processing() is called
     * and reset when the network finishes playing or is cancelled.
     */
    std::shared_ptr<Nodes::Network::NodeNetwork> m_current_network;

    /**
     * @brief The currently active buffer being played
     *
     * This pointer holds the buffer that is currently being played.
     * It is set when play_for() or play_with_processing() is called
     * and reset when the buffer finishes playing or is cancelled.
     */
    std::shared_ptr<Buffers::Buffer> m_current_buffer;

    /**
     * @brief The processing token associated with the currently active node or buffer
     *
     * This token is used to identify the processing context for the active node or buffer.
     * It is set when play_for() or play_with_processing() is called and reset when the
     * node or buffer finishes playing or is cancelled.
     */
    Nodes::ProcessingToken m_node_token;

    /**
     * @brief The processing token associated with the currently active buffer
     *
     * This token is used to identify the processing context for the active buffer.
     * It is set when play_for() or play_with_processing() is called and reset when the
     * buffer finishes playing or is cancelled.
     */
    Buffers::ProcessingToken m_buffer_token;

    /**
     * @brief The output channels the current node is connected to
     *
     * This vector stores the output channel numbers that the current
     * node is connected to. It is set when play_for() or play_with_processing()
     * is called and reset when the node finishes playing or is cancelled.
     */
    std::vector<uint32_t> m_channels;

    enum class ActiveType : uint8_t { NONE,
        NODE,
        BUFFER,
        NETWORK };

    ActiveType m_active_type = ActiveType::NONE;
};

}
