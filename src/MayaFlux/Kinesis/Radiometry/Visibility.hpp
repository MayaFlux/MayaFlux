#pragma once

#include "MayaFlux/Kinesis/Spatial/BoxHierarchy.hpp"

namespace MayaFlux::Kinesis {

/**
 * @class TriangleOccluder
 * @brief Indexed triangles that hide points from one another.
 *
 * Copies what it is given and is not updated; build another when they move.
 * Triangles hide from both sides. Queries may run from any number of threads.
 */
class MAYAFLUX_API TriangleOccluder {
public:
    /**
     * @param positions Vertex positions.
     * @param indices   Three per triangle; one naming a missing vertex hides nothing.
     */
    TriangleOccluder(std::span<const glm::vec3> positions, std::span<const uint32_t> indices);

    /** @brief Whether a triangle lies strictly between two points. */
    [[nodiscard]] bool occluded(const glm::vec3& from, const glm::vec3& to) const;

    /** @brief A distance that lifts a point clear of the surface it lies on. */
    [[nodiscard]] float clearance() const noexcept { return m_clearance; }

private:
    std::vector<glm::vec3> m_positions;
    std::vector<uint32_t> m_indices;
    BoxHierarchy m_hierarchy;
    float m_clearance;
};

/**
 * @brief Share of the cosine weighted hemisphere above a point that nothing
 *        hides within a range: one in the open, zero when covered.
 *
 * @param occluder What hides.
 * @param point    On a surface.
 * @param normal   Unit surface normal.
 * @param range    How far to look.
 * @param samples  Directions tried.
 * @param seed     Turns the directions, so neighbouring points do not share them.
 */
[[nodiscard]] MAYAFLUX_API float ambient_visibility(
    const TriangleOccluder& occluder, const glm::vec3& point, const glm::vec3& normal,
    float range, uint32_t samples = 64, uint32_t seed = 0);

/**
 * @brief ambient_visibility() for every point, in parallel.
 * @return One value per point, for as many as have a normal.
 */
[[nodiscard]] MAYAFLUX_API std::vector<float> ambient_visibility(
    const TriangleOccluder& occluder, std::span<const glm::vec3> points, std::span<const glm::vec3> normals,
    float range, uint32_t samples = 64);

/**
 * @brief Share of the points of a source that a point sees, for soft shadows.
 *
 * @param occluder What hides.
 * @param point    On a surface.
 * @param normal   Unit surface normal; the point is lifted along it.
 * @param targets  Points over the source. One target gives a hard shadow.
 * @return One when there are no targets.
 */
[[nodiscard]] MAYAFLUX_API float source_visibility(
    const TriangleOccluder& occluder, const glm::vec3& point, const glm::vec3& normal,
    std::span<const glm::vec3> targets);

/**
 * @brief How much of the specular reflection of the surroundings survives an
 *        ambient visibility (Lagarde).
 */
[[nodiscard]] inline float specular_occlusion(float cos_view, float visibility, float roughness) noexcept
{
    const float exponent = std::exp2(-16.0F * roughness - 1.0F);
    return std::clamp(
        std::pow(std::max(cos_view + visibility, 0.0F), exponent) - 1.0F + visibility, 0.0F, 1.0F);
}

} // namespace MayaFlux::Kinesis
