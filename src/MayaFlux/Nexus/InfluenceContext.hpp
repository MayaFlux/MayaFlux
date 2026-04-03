#pragma once

#include <glm/vec3.hpp>

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
    glm::vec3 position {};

    // @note future: ShaderProcessor* shader_proc { nullptr };
    // @note future: const std::vector<double>* audio_snapshot { nullptr };
    // @note future: RenderProcessor* render_proc { nullptr };
};

} // namespace MayaFlux::Nexus
