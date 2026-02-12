#pragma once

#include "Domain.hpp"
#include "MayaFlux/Kriya/Timers.hpp"

namespace MayaFlux {

/**
 * @class TimeSpec
 * @brief Represents a timed activation operation for processing nodes, buffers, or networks.
 *
 * The TimeSpec struct encapsulates the concept of activating a processing entity
 * for a specific duration and (optionally) on specific channels. It's designed to be used
 * with the stream operator (>>) and TemporalWrapper to create a fluent, expressive syntax
 * for computational flow programming.
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
 * ```
 *
 * The TimeSpec is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
struct MAYAFLUX_API TimeSpec {
    double seconds;
    std::optional<std::vector<uint32_t>> channels;

    /**
     * @brief Constructs a TimeSpec with the specified duration.
     * @param s Duration of the operation in seconds.
     */
    TimeSpec(double s)
        : seconds(s)
    {
    }
    /**
     * @brief Constructs a TimeSpec with the specified duration and channels.
     * @param s Duration of the operation in seconds.
     * @param ch List of channels to activate.
     */
    TimeSpec(double s, std::vector<uint32_t> ch)
        : seconds(s)
        , channels(ch)
    {
    }
    /**
     * @brief Constructs a TimeSpec with the specified duration and a single channel.
     * @param s Duration of the operation in seconds.
     * @param ch Channel to activate.
     */
    TimeSpec(double s, uint32_t ch)
        : seconds(s)
        , channels(std::vector<uint32_t> { ch })
    {
    }
};

/**
 * @class TemporalWrapper
 * @brief Wraps an entity with a TimeSpec for temporal activation.
 *
 * The TemporalWrapper class enables a fluent, expressive syntax for
 * activating processing entities (nodes, buffers, networks) for a specific duration,
 * using the stream operator (>>).
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
template <typename T>
class TemporalWrapper {
public:
    TemporalWrapper(const std::shared_ptr<T>& entity, TimeSpec spec)
        : m_entity(entity)
        , m_spec(std::move(spec))
    {
    }

    template <typename U>
        requires std::derived_from<U, T>
    TemporalWrapper(const TemporalWrapper<U>& other)
        : m_entity(other.entity())
        , m_spec(other.spec())
    {
    }

    std::shared_ptr<T> entity() const { return m_entity; }
    [[nodiscard]] const TimeSpec& spec() const { return m_spec; }

private:
    std::shared_ptr<T> m_entity;
    TimeSpec m_spec;
};

/**
 * @brief Creates a TimeSpec with the specified duration and a single channel.
 *
 * This overload allows specifying a single channel to activate.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds on channel 1
 * process_node >> Time(2.0, 1);
 * // Activate a processing node for 2 seconds on default 0 channel
 * process_node >> Time(2.0);
 * ```
 *
 * @param seconds Duration in seconds.
 * @param channel Channel to activate.
 * @return A TimeSpec object representing the specified duration and channel.
 */
MAYAFLUX_API TimeSpec Time(double seconds, uint32_t channel = 0);

/**
 * @brief Creates a TimeSpec with the specified duration and a list of channels.
 *
 * This overload allows specifying multiple channels to activate.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds on channels 0 and 1
 * process_node >> Time(2.0, {0, 1});
 * ```
 *
 * @param seconds Duration in seconds.
 * @param channels List of channels to activate.
 * @return A TimeSpec object representing the specified duration and channels.
 */
MAYAFLUX_API TimeSpec Time(double seconds, std::vector<uint32_t> channels);

/**
 * @brief Creates a TemporalWrapper for an entity and a TimeSpec.
 *
 * This operator overload implements the entity >> Time(seconds) syntax, which
 * wraps the entity with a TimeSpec for subsequent temporal activation.
 *
 * Example usage:
 * ```cpp
 * // Activate a processing node for 2 seconds
 * process_node >> Time(2.0);
 * ```
 *
 * This is part of a broader pattern of using operator overloading to create
 * a domain-specific language for computational flow programming within C++.
 *
 * @tparam T The type of the entity to wrap.
 * @param entity The entity to activate.
 * @param spec The TimeSpec specifying the duration and channels.
 * @return A TemporalWrapper<T> representing the timed activation.
 */
template <typename T>
TemporalWrapper<T> operator>>(std::shared_ptr<T> entity, const TimeSpec& spec)
{
    return TemporalWrapper<T>(entity, spec);
}

/**
 * @brief Activates a node in a specific domain for the duration specified by the TemporalWrapper.
 *
 * This operator overload implements the syntax:
 * ```cpp
 * process_node >> Time(2.0) | domain;
 * ```
 * It activates the wrapped node in the given domain for the specified duration and channels.
 *
 * @param wrapper The TemporalWrapper containing the node and timing information.
 * @param domain The domain in which to activate the node.
 * @return A shared pointer to the activated node.
 */
MAYAFLUX_API std::shared_ptr<Nodes::Node> operator|(const TemporalWrapper<Nodes::Node>& wrapper, Domain domain);

/**
 * @brief Activates a buffer in a specific domain for the duration specified by the TemporalWrapper.
 *
 * This operator overload implements the syntax:
 * ```cpp
 * buffer >> Time(2.0, 1) | domain;
 * ```
 * It activates the wrapped buffer in the given domain for the specified duration and channel(s).
 *
 * @param wrapper The TemporalWrapper containing the buffer and timing information.
 * @param domain The domain in which to activate the buffer.
 * @return A shared pointer to the activated buffer.
 */
MAYAFLUX_API std::shared_ptr<Buffers::Buffer> operator|(const TemporalWrapper<Buffers::Buffer>& wrapper, Domain domain);

/**
 * @brief Activates a node network in a specific domain for the duration specified by the TemporalWrapper.
 *
 * This operator overload implements the syntax:
 * ```cpp
 * network >> Time(2.0, {0, 1}) | domain;
 * ```
 * It activates the wrapped node network in the given domain for the specified duration and channels.
 *
 * @param wrapper The TemporalWrapper containing the node network and timing information.
 * @param domain The domain in which to activate the network.
 * @return A shared pointer to the activated node network.
 */
MAYAFLUX_API std::shared_ptr<Nodes::Network::NodeNetwork> operator|(const TemporalWrapper<Nodes::Network::NodeNetwork>& wrapper, Domain domain);

}
