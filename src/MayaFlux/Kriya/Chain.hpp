#pragma once

#include "MayaFlux/Utils.hpp"
#include "Operators.hpp"

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

/**
 * @class ActionToken
 * @brief A token representing an action in a computational sequence
 *
 * The ActionToken class represents a single action in a sequence, which can be
 * a node connection, a time delay, or a function call. It's designed to be used
 * with the Sequence class to create expressive sequences of computational operations.
 *
 * This approach is inspired by dataflow programming and reactive systems,
 * which use similar concepts to represent sequences of computational events. It allows for
 * a more intuitive, declarative way of expressing sequences compared to traditional
 * imperative programming.
 *
 * ActionTokens are typically created implicitly through conversions from nodes,
 * time values, or functions, making the API more concise and expressive.
 */
class MAYAFLUX_API ActionToken {
public:
    /**
     * @brief Constructs an ActionToken representing a node connection
     * @param _node The processing node to connect
     *
     * Creates a token that represents connecting a processing node to the output
     * or applying some other operation to it.
     */
    ActionToken(std::shared_ptr<Nodes::Node> _node);

    /**
     * @brief Constructs an ActionToken representing a time delay
     * @param _seconds The delay duration in seconds
     *
     * Creates a token that represents waiting for a specified amount of time
     * before proceeding to the next action in the sequence.
     */
    ActionToken(double _seconds);

    /**
     * @brief Constructs an ActionToken representing a function call
     * @param _func The function to call
     *
     * Creates a token that represents calling a function as part of the sequence.
     * This is useful for triggering state changes or control logic.
     */
    ActionToken(std::function<void()> _func);

    /**
     * @brief The type of action this token represents
     *
     * This field indicates whether the token represents a node connection,
     * a time delay, or a function call.
     */
    Utils::ActionType type;

    /**
     * @brief The processing node to connect (for NODE type tokens)
     *
     * This field contains the processing node to connect, if this token
     * represents a node connection.
     */
    std::shared_ptr<Nodes::Node> node;

    /**
     * @brief The function to call (for FUNCTION type tokens)
     *
     * This field contains the function to call, if this token
     * represents a function call.
     */
    std::function<void()> func;

    /**
     * @brief The delay duration in seconds (for TIME type tokens)
     *
     * This field contains the delay duration, if this token
     * represents a time delay.
     */
    double seconds = 0.F;
};

/**
 * @class Sequence
 * @brief A sequence of computational operations with a fluent, declarative API
 *
 * The Sequence class provides a way to create sequences of computational operations
 * using a fluent, declarative API with the stream operator (>>). It's designed
 * for creating expressive sequences of node connections, time delays, and
 * function calls.
 *
 * This approach is inspired by dataflow programming and reactive systems, which
 * use similar operators to express both data flow and temporal sequencing.
 * It allows for a more intuitive, declarative way of expressing sequences compared
 * to traditional imperative programming.
 *
 * Example usage:
 * ```cpp
 * // Create a sequence
 * Sequence seq;
 *
 * // Define a sequence of operations
 * seq >> data_source >> 0.5 >> []() { std::cout << "Half second passed" << std::endl; }
 *     >> transform_node >> 0.5 >> output_node;
 *
 * // Execute the sequence
 * seq.execute();
 * ```
 *
 * This creates a sequence that:
 * 1. Connects data_source to the output
 * 2. Waits 0.5 seconds
 * 3. Prints a message
 * 4. Connects transform_node to the output
 * 5. Waits 0.5 seconds
 * 6. Connects output_node to the output
 *
 * The Sequence is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
class MAYAFLUX_API Sequence {
public:
    /**
     * @brief Adds an action to the sequence
     * @param token The action to add
     * @return Reference to this Sequence for method chaining
     *
     * This operator overload implements the seq >> action syntax, which adds
     * an action to the sequence. The action can be a node connection, a time
     * delay, or a function call, represented by an ActionToken.
     *
     * The method returns a reference to the Sequence itself, allowing for a
     * fluent, declarative API style.
     */
    Sequence& operator>>(const ActionToken& token);

    /**
     * @brief Executes the sequence using the global managers
     *
     * This method executes the sequence of actions using the global node manager
     * and scheduler. It's convenient for simple cases where only one processing
     * engine is active.
     *
     * The actions are executed in the order they were added, with any time
     * delays respected.
     */
    void execute();

    /**
     * @brief Executes the sequence with explicit managers
     * @param node_manager The NodeGraphManager to use for node connections
     * @param scheduler The TaskScheduler to use for timing
     *
     * This method executes the sequence of actions using the provided node manager
     * and scheduler. This allows for more control over which managers are used,
     * which is useful in contexts where multiple processing engines might exist.
     *
     * The actions are executed in the order they were added, with any time
     * delays respected.
     */
    void execute(const std::shared_ptr<Nodes::NodeGraphManager>& node_manager, const std::shared_ptr<Vruta::TaskScheduler>& scheduler);

private:
    /**
     * @brief Collection of actions in this sequence
     *
     * This vector contains all the actions that have been added to the sequence,
     * in the order they will be executed.
     */
    std::vector<ActionToken> tokens;
};

}
