#pragma once

#include "Timers.hpp"

namespace MayaFlux::Kriya {

/**
 * @class TimeOperation
 * @brief Represents a timed activation operation for processing nodes
 *
 * The TimeOperation class encapsulates the concept of activating a processing node
 * for a specific duration. It's designed to be used with the stream operator (>>)
 * to create a fluent, expressive syntax for computational flow programming.
 *
 * This approach is inspired by flow-based programming paradigms, which use
 * operators to express data flow and timing relationships. It allows for a more
 * intuitive, declarative way of expressing temporal operations compared to traditional
 * function calls.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds
 * process_node >> Time(2.0);
 *
 * // Equivalent to:
 * // NodeTimer timer(*scheduler, *graph_manager);
 * // timer.play_for(process_node, 2.0);
 * ```
 *
 * The TimeOperation is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
class TimeOperation {
public:
    /**
     * @brief Constructs a TimeOperation with the specified duration
     * @param seconds Duration of the operation in seconds
     *
     * This constructor is typically used internally by the Time() factory function.
     * It creates a TimeOperation that will use the global scheduler and graph manager.
     */
    TimeOperation(double seconds);

    /**
     * @brief Constructs a TimeOperation with explicit scheduler and graph manager
     * @param seconds Duration of the operation in seconds
     * @param scheduler The TaskScheduler to use for timing
     * @param graph_manager The NodeGraphManager to use for node connections
     *
     * This constructor allows for more control over which scheduler and graph manager
     * are used, which is useful in contexts where multiple processing engines might exist.
     */
    TimeOperation(double seconds, Core::Scheduler::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager);

    /**
     * @brief Gets the duration of this operation
     * @return Duration in seconds
     *
     * This method returns the time duration associated with this operation,
     * which determines how long a node will remain active when connected to this operation.
     */
    inline double get_seconds() const { return m_seconds; }

private:
    /**
     * @brief Duration of the operation in seconds
     *
     * This value determines how long a node will remain active when connected to this operation.
     */
    double m_seconds;

    /**
     * @brief Reference to the scheduler that will manage timing
     *
     * The scheduler provides the timing infrastructure needed for
     * precise control of node activation durations.
     */
    Core::Scheduler::TaskScheduler& m_scheduler;

    /**
     * @brief Reference to the graph manager that will manage node connections
     *
     * The graph manager provides the infrastructure for connecting
     * and disconnecting nodes from the processing graph.
     */
    Nodes::NodeGraphManager& m_graph_manager;

    /**
     * @brief Grants the stream operator access to private members
     *
     * This friendship declaration allows the stream operator to access
     * the private members of TimeOperation, which is necessary for
     * implementing the node >> time syntax.
     */
    friend void operator>>(std::shared_ptr<Nodes::Node>, const TimeOperation&);
};

/**
 * @class OutputTerminal
 * @brief Represents a terminal output sink in the processing graph
 *
 * The OutputTerminal class is a singleton that represents the final output of the system.
 * It's designed to be used with the stream operator (>>) to create a fluent,
 * expressive syntax for connecting nodes to the system output.
 *
 * This approach is inspired by flow-based programming paradigms, which use
 * similar concepts to represent terminal sinks. It allows for a more
 * intuitive way of expressing data flow compared to traditional function calls.
 *
 * Example usage:
 * ```cpp
 * // Connect a processing node to the system output
 * process_node >> OutputTerminal::instance();
 *
 * // Connect to a specific output channel
 * OutputTerminal::instance().channel = 1;
 * process_node >> OutputTerminal::instance();
 * ```
 *
 * The OutputTerminal is part of a broader pattern of using operator overloading to create
 * a domain-specific language for computational flow programming within C++.
 */
class DAC {
public:
    /**
     * @brief Gets the singleton instance of OutputTerminal
     * @return Reference to the OutputTerminal singleton
     *
     * This method provides access to the singleton instance of OutputTerminal,
     * which represents the system output.
     */
    static DAC& instance()
    {
        static DAC dac;
        return dac;
    }

    /**
     * @brief The output channel to connect to
     *
     * This value determines which system output channel a node will be
     * connected to when using the stream operator. The default is 0,
     * which typically corresponds to the primary output channel.
     */
    unsigned int channel = 0;

private:
    /**
     * @brief Private constructor to enforce singleton pattern
     *
     * The OutputTerminal constructor is private to prevent direct instantiation,
     * enforcing the singleton pattern through the instance() method.
     */
    DAC() = default;
};

/**
 * @brief Connects a processing node to the system output
 * @param node The processing node to connect
 * @param terminal The OutputTerminal instance representing the system output
 *
 * This operator overload implements the node >> OutputTerminal syntax, which connects
 * a processing node to the system's output. It's a more expressive way
 * of representing data flow compared to traditional function calls.
 *
 * Example usage:
 * ```cpp
 * // Connect a processing node to the system output
 * process_node >> OutputTerminal::instance();
 * ```
 *
 * This is part of a broader pattern of using operator overloading to create
 * a domain-specific language for computational flow programming within C++.
 */
void operator>>(std::shared_ptr<Nodes::Node> node, DAC& terminal);

/**
 * @brief Activates a processing node for a specific duration
 * @param node The processing node to activate
 * @param time_op The TimeOperation specifying the duration
 *
 * This operator overload implements the node >> Time(seconds) syntax, which
 * activates a processing node for a specific duration. It's a more expressive way
 * of representing timed activation compared to traditional function calls.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds
 * process_node >> Time(2.0);
 * ```
 *
 * This is part of a broader pattern of using operator overloading to create
 * a domain-specific language for computational flow programming within C++.
 */
void operator>>(std::shared_ptr<Nodes::Node> node, const TimeOperation& time_op);

/**
 * @brief Creates a TimeOperation with the specified duration
 * @param seconds Duration in seconds
 * @return A TimeOperation object representing the specified duration
 *
 * This factory function creates a TimeOperation with the specified duration,
 * using the global scheduler and graph manager. It's designed to be used
 * with the stream operator to create a fluent, expressive syntax for
 * timed node activation.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds
 * process_node >> Time(2.0);
 * ```
 *
 * This function is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
TimeOperation Time(double seconds);

}
