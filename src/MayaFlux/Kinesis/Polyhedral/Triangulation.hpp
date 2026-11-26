#pragma once

#include "Incidence.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @struct Triangulation
 * @brief A face set decomposed into triangles that keep each face's winding.
 *
 * Faces appear in order. Every face that succeeded contributes size - 2 triangles, and a
 * face listed in failed_faces contributes none.
 */
struct Triangulation {
    std::vector<uint32_t> indices; ///< Three vertices per triangle.
    std::vector<uint32_t> triangle_face; ///< The face each triangle came from.
    std::vector<uint32_t> failed_faces; ///< Faces with no usable normal or a self crossing outline.
};

/**
 * @brief Decompose every face into triangles.
 *
 * A triangle is kept as it is. A convex quadrilateral splits along one diagonal. Any other
 * convex face is a fan from its first corner. A concave face is projected onto the plane
 * of its Newell normal and clipped by Kinesis::triangulate_polygon. A quadrilateral that
 * cannot be clipped, such as one whose outline crosses itself, splits along a diagonal as
 * a convex one does, so one of its triangles may face the other way. Larger faces must be
 * a simple outline, and a face with no area fails. Faces are independent and run in parallel.
 *
 * @param shorter_quad_diagonal Split a convex quadrilateral along its shorter diagonal, the
 * first on a tie. False always splits from the first corner to the third.
 * @param convexity_tolerance As polygon_is_convex.
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Triangulation> triangulate_faces(
    std::span<const glm::vec3> positions,
    const Incidence& incidence,
    bool shorter_quad_diagonal = true,
    float convexity_tolerance = 1.0e-6F);

} // namespace MayaFlux::Kinesis
