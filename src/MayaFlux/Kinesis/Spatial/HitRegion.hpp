#pragma once

#include "Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct HitRegion
 * @brief A region a point can hit: fast reject bounds, an authoritative
 *        containment test, and an optional clip.
 *
 * The three parts are independent, so any bounds function composes with any
 * box. hits() applies them in one fixed order:
 * - a point outside clip never hits;
 * - a point outside bounds never hits, so bounds must circumscribe contains;
 * - contains, when set, decides;
 * - otherwise bounds alone decides, and a region with neither never hits.
 *
 * Dimension aliases follow the box family: HitRegion1D with AABB1D over a
 * scalar coordinate, HitRegion2D with AABB2D, HitRegion3D with AABB3D.
 *
 * Usage:
 * @code
 * HitRegion2D disc {
 *     .bounds = AABB2D::from_ndc({ 0.0F, 0.0F }, { 0.3F, 0.3F }),
 *     .contains = circular_bounds({ 0.0F, 0.0F }, 0.3F),
 * };
 * bool inside = disc.hits({ 0.1F, 0.1F });
 * @endcode
 *
 * @tparam Point Coordinate type: double, glm::vec2 or glm::vec3.
 * @tparam Box   Box type with contains(Point), as AABB1D, AABB2D and AABB3D have.
 */
template <typename Point, typename Box>
struct HitRegion {
    /// @brief Fast reject box; must circumscribe contains when both are set.
    std::optional<Box> bounds;

    /// @brief Authoritative containment test.
    std::function<bool(Point)> contains;

    /// @brief Region outside which nothing hits, regardless of the other parts.
    std::optional<Box> clip;

    /** @brief Whether a point hits this region. */
    [[nodiscard]] bool hits(Point p) const
    {
        if (clip && !clip->contains(p))
            return false;
        if (bounds && !bounds->contains(p))
            return false;
        if (contains)
            return contains(p);
        return bounds.has_value();
    }
};

using HitRegion1D = HitRegion<double, AABB1D>;
using HitRegion2D = HitRegion<glm::vec2, AABB2D>;
using HitRegion3D = HitRegion<glm::vec3, AABB3D>;

}
