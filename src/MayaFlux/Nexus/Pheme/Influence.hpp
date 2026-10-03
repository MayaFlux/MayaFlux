#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace MayaFlux::Nexus {

/**
 * @struct InfluenceUBO
 * @brief GPU-side std140 layout matching InfluenceContext plain data fields.
 *
 * Packs into two vec4 slots plus one partial (48 bytes total).
 * GLSL declaration:
 * @code
 * layout(set = 1, binding = 1) uniform Influence {
 *     vec3  position;
 *     float intensity;
 *     vec3  color;
 *     float radius;
 *     float size;
 * };
 * @endcode
 */
struct alignas(16) InfluenceUBO {
    glm::vec3 position { 0.0F };
    float intensity { 1.0F };
    glm::vec3 color { 1.0F, 1.0F, 1.0F };
    float radius { 1.0F };
    float size { 1.0F };
    float _pad[3] {};
};

static_assert(sizeof(InfluenceUBO) == 48, "InfluenceUBO must be 48 bytes for std140 alignment");

/**
 * @struct InfluenceContext
 * @brief Data passed to an Emitter or Agent influence function on each commit.
 *
 * Only the fields relevant to the active execution path are populated.
 * Fields marked @note future are reserved for later domain expansions and
 * are always default-initialised in the current implementation.
 */
struct InfluenceContext {
    glm::vec3 position {}; ///< Position of the influence point in world space.

    float intensity { 1.0F }; ///< Intensity of the influence, typically in the range [0, 1], but may exceed 1 for strong influences.

    float radius { 1.0F }; ///< Radius of influence around the position, defining the area of effect for spatially-dependent influences.

    std::optional<glm::vec3> color; ///< Optional color hint for the influence, may be used for visualisation or shader effects.

    std::optional<float> size; ///< Optional size hint for the influence, may be used for visualisation or shader effects.

    std::optional<glm::vec2> cursor_pos; ///< Optional cursor position for pending interactions, useful for real-time editing or previewing influence effects.

    std::optional<glm::quat> orientation; ///< Optional orientation of the influencer. Local +Z is its forward direction.

    // @note future: ShaderProcessor* shader_proc { nullptr };
    // @note future: std::span<const double> audio_snapshot;
};

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
