#pragma once

#include "Incidence.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @enum QuadDiagonal
 * @brief Which diagonal splits a convex quadrilateral.
 */
enum class QuadDiagonal : uint8_t {
    First, ///< From the first corner to the third.
    Shorter ///< Whichever diagonal is shorter, the first on a tie.
};

/**
 * @struct TriangulationOptions
 * @brief Choices a triangulation makes where the face leaves them open.
 */
struct TriangulationOptions {
    QuadDiagonal quad_diagonal { QuadDiagonal::Shorter };
    float convexity_tolerance { 1.0e-6F }; ///< As polygon_is_convex.
};

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
 * of its Newell normal and clipped by Kinesis::triangulate_polygon, so it must be a
 * simple outline. Faces are independent and run in parallel.
 *
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Triangulation> triangulate_faces(
    std::span<const glm::vec3> positions,
    const Incidence& incidence,
    const TriangulationOptions& options = {});

} // namespace MayaFlux::Kinesis
