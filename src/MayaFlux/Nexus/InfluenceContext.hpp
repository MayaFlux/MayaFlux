#pragma once

#include <glm/vec3.hpp>

namespace MayaFlux::Buffers {
class RenderProcessor;
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

    // @note future: ShaderProcessor* shader_proc { nullptr };
    // @note future: std::span<const double> audio_snapshot;

    std::weak_ptr<Buffers::RenderProcessor> render_proc;
};

} // namespace MayaFlux::Nexus
