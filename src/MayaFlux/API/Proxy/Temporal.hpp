#pragma once

#include "Creator.hpp"
#include "MayaFlux/Kriya/Timers.hpp"

namespace MayaFlux {

/**
 * @struct TimeSpec
 * @brief Duration operand for the entity >> Time(s) | DomainSpec syntax.
 *
 * The TimeSpec struct encapsulates the concept of activating a processing entity
 * for a specific duration. It's designed to be used
 * with the stream operator (>>) and TemporalWrapper to create a fluent, expressive syntax
 * for computational flow programming.
 *
 * This approach is inspired by flow-based programming paradigms, which use
 * operators to express data flow and timing relationships. It allows for a more
 * intuitive, declarative way of expressing temporal operations compared to traditional
 * function calls.
 *
 * Carries only the activation duration. Channel binding is expressed at the
 * domain site via DomainSpec::operator[]:
 * @code
 * node    >> Time(2.0) | Audio[0];
 * node    >> Time(2.0) | Audio[{0, 1}];
 * buffer  >> Time(2.0) | Audio[0];
 * network >> Time(2.0) | Graphics;
 * @endcode
 * ```
 *
 * The TimeSpec is part of a broader pattern of using operator overloading
 * to create a domain-specific language for computational flow programming within C++.
 */
struct MAYAFLUX_API TimeSpec {
    double seconds;

    /**
     * @brief Constructs a TimeSpec with the specified duration.
     * @param s Duration of the activation in seconds.
     */
    explicit TimeSpec(double s)
        : seconds(s)
    {
    }
};

/**
 * @class TemporalWrapper
 * @brief Intermediate produced by operator>> binding an entity to a TimeSpec.
 *
 * Consumed by operator|(TemporalWrapper, CreationContext) which resolves the
 * domain and channel binding expressed by the right-hand DomainSpec.
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
 * @brief Constructs a TimeSpec for the given duration.
 *
 * @code
 * node >> Time(2.0) | Audio[0];
 * @endcode
 *
 * @param seconds Duration in seconds.
 * @return TimeSpec for use with operator>>.
 */
MAYAFLUX_API TimeSpec Time(double seconds);

/**
 * @brief Wraps an entity and a TimeSpec into a TemporalWrapper.
 *
 * @tparam T Entity type (Node, Buffer, or NodeNetwork).
 * @param entity The entity to activate.
 * @param spec   The duration specification.
 * @return TemporalWrapper<T> ready for operator|.
 */
template <typename T>
TemporalWrapper<T> operator>>(std::shared_ptr<T> entity, const TimeSpec& spec)
{
    return TemporalWrapper<T>(entity, spec);
}

/**
 * @brief Activates a node in the domain and channel(s) expressed by ctx.
 *
 * @code
 * node >> Time(2.0) | Audio[0];
 * node >> Time(2.0) | Audio[{0, 1}];
 * @endcode
 *
 * @param wrapper TemporalWrapper carrying the node and duration.
 * @param ctx     CreationContext produced by DomainSpec or DomainSpec::operator[].
 * @return Shared pointer to the activated node.
 */
MAYAFLUX_API std::shared_ptr<Nodes::Node> operator|(
    const TemporalWrapper<Nodes::Node>& wrapper,
    const CreationContext& ctx);

/**
 * @brief Activates a buffer in the domain and channel expressed by ctx.
 *
 * @code
 * buffer >> Time(2.0) | Audio[0];
 * @endcode
 *
 * @param wrapper TemporalWrapper carrying the buffer and duration.
 * @param ctx     CreationContext produced by DomainSpec or DomainSpec::operator[].
 * @return Shared pointer to the activated buffer.
 */
MAYAFLUX_API std::shared_ptr<Buffers::Buffer> operator|(
    const TemporalWrapper<Buffers::Buffer>& wrapper,
    const CreationContext& ctx);

/**
 * @brief Activates a node network in the domain and channel(s) expressed by ctx.
 *
 * @code
 * network >> Time(2.0) | Audio[{0, 1}];
 * network >> Time(2.0) | Graphics;
 * @endcode
 *
 * @param wrapper TemporalWrapper carrying the network and duration.
 * @param ctx     CreationContext produced by DomainSpec or DomainSpec::operator[].
 * @return Shared pointer to the activated network.
 */
MAYAFLUX_API std::shared_ptr<Nodes::Network::NodeNetwork> operator|(
    const TemporalWrapper<Nodes::Network::NodeNetwork>& wrapper,
    const CreationContext& ctx);

} // namespace MayaFlux
