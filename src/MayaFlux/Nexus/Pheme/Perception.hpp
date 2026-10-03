#pragma once

#include "MayaFlux/Kinesis/Spatial/SpatialIndex.hpp"

#include <glm/gtc/quaternion.hpp>

namespace MayaFlux::Nexus {

/**
 * @struct PerceptionContext
 * @brief Data passed to a Sensor or Agent perception function on each commit.
 *
 * @c spatial_results contains the entities within @c radius of @c position
 * at the moment of the last published snapshot. Results hold entity ids and
 * squared distances as returned by @c SpatialIndex::within_radius.
 *
 * Fields marked @note future are reserved for later domain expansions.
 */
struct PerceptionContext {
    glm::vec3 position {};
    float radius { 1.0F }; ///< Query radius the spatial results were gathered with.
    std::optional<glm::quat> orientation; ///< Optional orientation of the perceiver. Local +Z is its forward direction.
    std::span<const Kinesis::QueryResult> spatial_results;

    // @note future: EnergyAnalysis audio_energy {};
    // @note future: std::span<const double> audio_snapshot {};
};

/**
 * @class Perception
 * @brief A value of any type, read from the world through the perception context and handed to its targets.
 *
 * T is whatever is sensed: a scalar from a node or buffer, a vector, a struct
 * of readings, a Tendency, a DataVariant. The source reads it, using the
 * context when it needs position or neighbours. A target is any callable taking
 * const T& and decides where it lands, such as a cell the perception function
 * reads. On each invoke the source runs once and every target gets the value,
 * in the order they were added.
 *
 * How many targets a Perception may have is decided by its owner: a Sensor
 * keeps one, an Agent keeps as many as are added.
 */
template <typename T>
class Perception {
public:
    using Source = std::function<T(const PerceptionContext&)>;
    using Target = std::function<void(const T&)>;

    explicit Perception(Source source)
        : m_source(std::move(source))
    {
    }

    /** @brief Add a target. Returns this perception for chaining. */
    Perception& add_target(Target target)
    {
        m_targets.push_back(std::move(target));
        return *this;
    }

    /** @brief Remove every target. */
    void clear_targets() { m_targets.clear(); }

    /** @brief Number of targets currently added. */
    [[nodiscard]] size_t target_count() const { return m_targets.size(); }

    /** @brief Read the value using @p ctx and hand it to every target. */
    void invoke(const PerceptionContext& ctx) const
    {
        if (!m_source) {
            return;
        }
        const T value = m_source(ctx);
        for (const auto& target : m_targets) {
            target(value);
        }
    }

private:
    Source m_source;
    std::vector<Target> m_targets;
};

} // namespace MayaFlux::Nexus
