#pragma once

#include "Timers.hpp"

namespace MayaFlux::Kriya {

/**
 * @class NodeTimeSpec
 * @brief Represents a timed activation operation for processing nodes
 *
 * The NodeTimeSpec class encapsulates the concept of activating a processing node
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
 * The NodeTimeSpec is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
class MAYAFLUX_API NodeTimeSpec {
public:
    /**
     * @brief Constructs a NodeTimeSpec with the specified duration
     * @param seconds Duration of the operation in seconds
     * @param channels Optional list of channels to activate; if not provided, all active channels are used
     *
     * This constructor is typically used internally by the Time() factory function.
     * It creates a NodeTimeSpec that will use the global scheduler and graph manager.
     */
    NodeTimeSpec(double seconds, std::optional<std::vector<uint32_t>> channels = std::nullopt);

    /**
     * @brief Constructs a NodeTimeSpec with explicit scheduler and graph manager
     * @param seconds Duration of the operation in seconds
     * @param scheduler The TaskScheduler to use for timing
     * @param graph_manager The NodeGraphManager to use for node connections
     *
     * This constructor allows for more control over which scheduler and graph manager
     * are used, which is useful in contexts where multiple processing engines might exist.
     */
    NodeTimeSpec(double seconds, Vruta::TaskScheduler& scheduler, Nodes::NodeGraphManager& graph_manager);

    /**
     * @brief Gets the duration of this operation
     * @return Duration in seconds
     *
     * This method returns the time duration associated with this operation,
     * which determines how long a node will remain active when connected to this operation.
     */
    [[nodiscard]] inline double get_seconds() const { return m_seconds; }

    /**
     * @brief Checks if explicit channels were specified
     * @return True if specific channels were provided, false if all active channels should be used
     *
     * This method indicates whether the operation was created with an explicit
     * list of channels to activate. If false, the operation will use all channels
     * that the node is currently active on.
     */
    [[nodiscard]] bool has_explicit_channels() const { return m_channels.has_value(); }

    /**
     * @brief Gets the list of channels to activate
     * @return Reference to the vector of channel indices
     *
     * This method returns the list of channels that were specified when creating
     * this operation. If no channels were specified, this method should not be called.
     */
    [[nodiscard]] const std::vector<uint32_t>& get_channels() const { return m_channels.value(); }

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
    Vruta::TaskScheduler& m_scheduler;

    /**
     * @brief Reference to the graph manager that will manage node connections
     *
     * The graph manager provides the infrastructure for connecting
     * and disconnecting nodes from the processing graph.
     */
    Nodes::NodeGraphManager& m_graph_manager;

    std::optional<std::vector<uint32_t>> m_channels;

    /**
     * @brief Grants the stream operator access to private members
     *
     * This friendship declaration allows the stream operator to access
     * the private members of NodeTimeSpec, which is necessary for
     * implementing the node >> time syntax.
     */
    friend void operator>>(std::shared_ptr<Nodes::Node>, const NodeTimeSpec&);
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
class MAYAFLUX_API DAC {
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
 * @param time_op The NodeTimeSpec specifying the duration
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
void operator>>(std::shared_ptr<Nodes::Node> node, const NodeTimeSpec& time_op);

/**
 * @brief Creates a NodeTimeSpec with the specified duration
 * @param seconds Duration in seconds
 * @param channels Optional list of channels to activate
 * @param channel Optional single channel to activate
 * @return A NodeTimeSpec object representing the specified duration
 *
 * This factory function creates a NodeTimeSpec with the specified duration,
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
NodeTimeSpec Time(double seconds, std::vector<uint32_t> channels);
NodeTimeSpec Time(double seconds, uint32_t channel);
NodeTimeSpec Time(double seconds);

}
