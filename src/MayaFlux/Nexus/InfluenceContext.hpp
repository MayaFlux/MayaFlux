#pragma once

#include <glm/vec3.hpp>

namespace MayaFlux::Buffers {
class RenderProcessor;
}

namespace MayaFlux::Nexus {

/**
 * @struct InfluenceUBO
 * @brief GPU-side std140 layout matching InfluenceContext plain data fields.
 *
 * Packs into two vec4 slots plus one partial (48 bytes total).
 * GLSL declaration:
 * @code
 * layout(set = 1, binding = 0) uniform Influence {
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

    // @note future: ShaderProcessor* shader_proc { nullptr };
    // @note future: std::span<const double> audio_snapshot;

    std::weak_ptr<Buffers::RenderProcessor> render_proc;
};

} // namespace MayaFlux::Nexus
