#pragma once

#include "InfluenceContext.hpp"

namespace MayaFlux::Nexus {

/**
 * @class Influence
 * @brief A value of any type, built from the influence context and handed to its targets.
 *
 * T is whatever the targets need: a struct for a shader block, a vector, a
 * matrix, a Tendency, a DataVariant. A target is any callable taking const T&
 * and decides what the value affects. On each invoke the producer runs once and
 * every target gets the value, in the order they were added.
 *
 * How many targets an Influence may have is decided by its owner: an Emitter
 * keeps one, an Agent keeps as many as are added.
 */
template <typename T>
class Influence {
public:
    using Producer = std::function<T(const InfluenceContext&)>;
    using Target = std::function<void(const T&)>;

    explicit Influence(Producer producer)
        : m_producer(std::move(producer))
    {
    }

    /** @brief Add a target. Returns this influence for chaining. */
    Influence& add_target(Target target)
    {
        m_targets.push_back(std::move(target));
        return *this;
    }

    /** @brief Remove every target. */
    void clear_targets() { m_targets.clear(); }

    /** @brief Number of targets currently added. */
    [[nodiscard]] size_t target_count() const { return m_targets.size(); }

    /** @brief Build the value from @p ctx and hand it to every target. */
    void invoke(const InfluenceContext& ctx) const
    {
        if (!m_producer) {
            return;
        }
        const T value = m_producer(ctx);
        for (const auto& target : m_targets) {
            target(value);
        }
    }

private:
    Producer m_producer;
    std::vector<Target> m_targets;
};

} // namespace MayaFlux::Nexus
