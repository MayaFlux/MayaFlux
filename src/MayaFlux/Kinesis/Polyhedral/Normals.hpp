#pragma once

#include "Incidence.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @enum NormalWeighting
 * @brief How much each face counts toward the normal at a vertex it touches.
 */
enum class NormalWeighting : uint8_t {
    Uniform, ///< Every face counts once.
    Area, ///< By the face's area.
    Angle ///< By the interior angle the face has at that vertex.
};

/**
 * @brief The normal at every vertex, from the faces around it.
 *
 * Each face contributes its unit normal scaled by the chosen weight, and the sum is made
 * unit length. Angle weighting makes the result independent of how a face is cut into
 * triangles. A vertex no face names, or whose contributions cancel, gets the zero vector.
 * Vertices are independent and run in parallel.
 *
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::vector<glm::vec3>> weighted_vertex_normals(
    std::span<const glm::vec3> positions,
    const Incidence& incidence,
    NormalWeighting weighting = NormalWeighting::Angle);

} // namespace MayaFlux::Kinesis
