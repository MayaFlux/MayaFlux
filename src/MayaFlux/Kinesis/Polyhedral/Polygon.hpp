#pragma once

#include "Incidence.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

namespace detail {

    /**
     * @brief Scale every vector to unit length, leaving a zero vector zero.
     */
    MAYAFLUX_API void normalize_in_place(std::span<glm::vec3> vectors);

} // namespace detail

/**
 * @brief Vector area of a polygon: its normal scaled by its area.
 *
 * Sums the cross products of the fan from the first vertex, in double precision and
 * relative to that vertex, so a polygon far from the origin keeps its accuracy and the
 * result does not depend on where the fan starts. For a polygon that is not flat it is
 * the area vector of the surface that spans the boundary with least projected loss
 * (Newell's normal). A cycle that winds counter clockwise about a direction has the
 * vector area along that direction.
 *
 * @param positions Vertex positions.
 * @param ring Indices into @p positions, in order around the polygon. Every index must
 * be valid. Fewer than three gives zero.
 */
[[nodiscard]] MAYAFLUX_API glm::vec3 polygon_area_vector(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring);

/** @brief Length of the vector area. */
[[nodiscard]] MAYAFLUX_API float polygon_area(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring);

/**
 * @brief Unit normal of a polygon, along its vector area.
 * @return nullopt when the vector area is zero or not finite.
 */
[[nodiscard]] MAYAFLUX_API std::optional<glm::vec3> polygon_normal(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring);

/**
 * @brief Centroid of the area a polygon encloses.
 *
 * Each fan triangle counts by its signed area along the normal, so a concave outline is
 * handled. A polygon of zero area gives the mean of its vertices.
 */
[[nodiscard]] MAYAFLUX_API glm::vec3 polygon_centroid(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring);

/**
 * @brief Farthest distance of a vertex from the plane through the centroid along the normal.
 * @return nullopt when the polygon has no normal. Zero for a triangle.
 */
[[nodiscard]] MAYAFLUX_API std::optional<float> polygon_planarity(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring);

/**
 * @brief Whether a polygon bends one way at every vertex and turns once around.
 *
 * A turn is judged against the normal, as the sine of the exterior angle times the two
 * edge lengths, so a turn more negative than @p tolerance of that product makes the
 * polygon concave. Collinear vertices are allowed and a zero length edge is skipped.
 * From five corners the exterior angles must also sum to one full turn, which rules out
 * star polygons. A triangle is convex when it has area. A polygon without a normal is
 * not convex.
 */
[[nodiscard]] MAYAFLUX_API bool polygon_is_convex(
    std::span<const glm::vec3> positions, std::span<const uint32_t> ring, float tolerance = 1.0e-6F);

/**
 * @brief Vector area of every face.
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::vector<glm::vec3>> face_area_vectors(
    std::span<const glm::vec3> positions, const Incidence& incidence);

/**
 * @brief Unit normal of every face. A face with no normal gets the zero vector.
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::vector<glm::vec3>> face_normals(
    std::span<const glm::vec3> positions, const Incidence& incidence);

/**
 * @brief Area of every face.
 * @return nullopt when @p positions has fewer entries than the incidence has vertices.
 */
[[nodiscard]] MAYAFLUX_API std::optional<std::vector<float>> face_areas(
    std::span<const glm::vec3> positions, const Incidence& incidence);

} // namespace MayaFlux::Kinesis
