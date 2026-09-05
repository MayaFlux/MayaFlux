#pragma once

#include "Tendency.hpp"

#include "MayaFlux/Kinesis/Stochastic/Stochastic.hpp"

namespace MayaFlux::Kinesis::ForceFields {

/**
 * @brief Radial attraction/repulsion toward an anchor point
 * @param anchor Target position
 * @param strength Force magnitude scalar (positive attracts, negative repels)
 * @return VectorField: glm::vec3 -> glm::vec3
 *
 * Force magnitude follows inverse square: strength / max(d², floor).
 * Direction is normalized delta from evaluation point to anchor.
 * Floor of 0.1 prevents singularity at the anchor position.
 */
inline VectorField point_attractor(const glm::vec3& anchor, float strength)
{
    return { .fn = [anchor, strength](const glm::vec3& p) -> glm::vec3 {
        glm::vec3 delta = anchor - p;
        float d = glm::length(delta);
        if (d < 0.001F)
            return glm::vec3(0.0F);
        return (delta / d) * (strength / std::max(d * d, 0.1F));
    } };
}

/**
 * @brief Tangential force perpendicular to both an axis and the radius vector.
 * @param anchor Point the orbit is centred on.
 * @param axis Orbital axis (e.g. glm::vec3(0, 1, 0) for orbits in the XZ plane).
 * @param strength Force magnitude, constant with distance.
 * @return VectorField: glm::vec3 -> glm::vec3
 *
 * cross(axis, p - anchor) is perpendicular to the radius vector in the plane
 * normal to axis, which is the direction that turns radial motion into
 * revolution rather than a straight collapse or escape. Combine with
 * point_attractor on the same anchor for an orbit: attraction supplies the
 * centripetal pull, this supplies the sideways push that keeps the fall from
 * being straight down. Unlike a true two-body problem this is force-based
 * rather than velocity-constrained, so orbits drift rather than close
 * exactly, which reads as organic rather than as a bug.
 */
inline VectorField orbital(const glm::vec3& anchor, const glm::vec3& axis, float strength)
{
    return { .fn = [anchor, axis, strength](const glm::vec3& p) -> glm::vec3 {
        glm::vec3 radius = p - anchor;
        glm::vec3 tangent = glm::cross(axis, radius);
        float d = glm::length(tangent);
        if (d < 0.001F)
            return glm::vec3(0.0F);
        return (tangent / d) * strength;
    } };
}

/**
 * @brief Uniform random force field using Stochastic infrastructure
 * @param strength Maximum magnitude per axis
 * @param rng Stochastic generator instance (captured by value, caller controls seed)
 * @return VectorField: glm::vec3 -> glm::vec3
 *
 * Produces a different random vector on each evaluation. Position input
 * is ignored: the field is spatially uniform but temporally varying.
 * For spatially coherent noise, compose with a Perlin/simplex generator.
 */
inline VectorField turbulence(float strength, Stochastic::Stochastic rng = Stochastic::Stochastic())
{
    return { .fn = [strength, rng](const glm::vec3&) mutable -> glm::vec3 {
        return glm::vec3(
                   rng(-1.0F, 1.0F),
                   rng(-1.0F, 1.0F),
                   rng(-1.0F, 1.0F))
            * strength;
    } };
}

} // namespace MayaFlux::Kinesis::ForceFields
