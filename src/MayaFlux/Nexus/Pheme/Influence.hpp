#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace MayaFlux::Core {
struct WindowEvent;
}

namespace MayaFlux::Nexus {

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

    std::optional<glm::quat> orientation; ///< Optional orientation of the influencer. Local -Z is its forward direction.

    std::optional<glm::vec2> cone; ///< Optional inner and outer angles of a cone about forward, radians from the axis.

    std::optional<glm::vec3> attenuation; ///< Optional constant, linear and quadratic terms dividing the fall with distance.

    const Core::WindowEvent* input {}; ///< The window event that caused this firing, valid only during the call; null for firings a window event did not cause.

    // @note future: ShaderProcessor* shader_proc { nullptr };
    // @note future: std::span<const double> audio_snapshot;
};

/**
 * @struct InfluenceBlock
 * @brief Host mirror of the Influence push constant block the library's lit shaders declare.
 *
 * Build it from a context with from() and push it with bind_push_constants.
 * @code
 * layout(push_constant) uniform Influence {
 *     vec3  position;
 *     float intensity;
 *     vec3  color;
 *     float radius;
 *     float size;
 *     vec3  direction;
 *     float cone_inner;
 *     vec3  attenuation;
 *     float cone_outer;
 * };
 * @endcode
 *
 * A shader of your own may declare the leading members only.
 */
struct alignas(16) InfluenceBlock {
    glm::vec3 position { 0.0F };
    float intensity { 1.0F };
    glm::vec3 color { 1.0F, 1.0F, 1.0F };
    float radius { 1.0F };
    float size { 1.0F };
    float _pad[3] {};
    glm::vec3 direction { 0.0F, 0.0F, -1.0F };
    float cone_inner { 0.0F };
    glm::vec3 attenuation { 1.0F, 0.0F, 0.0F };
    float cone_outer { 0.0F }; ///< Zero or less is no cone.

    /** @brief The block for @p ctx, with white, size 1, no cone and no attenuation where it has none. */
    [[nodiscard]] static InfluenceBlock from(const InfluenceContext& ctx)
    {
        const glm::vec2 cone = ctx.cone.value_or(glm::vec2(0.0F));
        return {
            .position = ctx.position,
            .intensity = ctx.intensity,
            .color = ctx.color.value_or(glm::vec3(1.0F)),
            .radius = ctx.radius,
            .size = ctx.size.value_or(1.0F),
            .direction = ctx.orientation.value_or(glm::quat(1.0F, 0.0F, 0.0F, 0.0F)) * glm::vec3(0.0F, 0.0F, -1.0F),
            .cone_inner = cone.x,
            .attenuation = ctx.attenuation.value_or(glm::vec3(1.0F, 0.0F, 0.0F)),
            .cone_outer = cone.y,
        };
    }
};

static_assert(sizeof(InfluenceBlock) == 80);

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
 *
 * Context is what the producer reads. Emitter and Agent supply an
 * InfluenceContext, an Expanse supplies a CrossingContext. Targets take only
 * const T&, so one target works with either.
 */
template <typename T, typename Context = InfluenceContext>
class Influence {
public:
    using Producer = std::function<T(const Context&)>;
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
    void invoke(const Context& ctx) const
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
