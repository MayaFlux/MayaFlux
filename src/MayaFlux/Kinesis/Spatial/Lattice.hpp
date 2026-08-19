#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct Lattice3D
 * @brief A regular subdivision of an AABB3D into a cell count per axis.
 *
 * Pairs the continuous extent AABB3D describes with the discrete
 * resolution a sampled field, a simulation grid, or an extraction pass
 * imposes on it. Everything else is derived: cell size, cell centre,
 * corner position, linear index.
 *
 * Indexing is x-major, y next, z outermost, matching the layout
 * volume_common.glsl assumes and the order VolumeGridBuffer allocates in.
 * A different traversal order is a different type, not a flag on this one.
 *
 * Cell-centred and corner-sampled are both expressed here rather than
 * chosen at construction: marching cubes reads corners of the same
 * lattice whose centres a simulation writes, and forcing that choice into
 * the type would require two lattices where one describes the geometry.
 */
struct Lattice3D {
    glm::uvec3 resolution { 1U }; ///< Cell count per axis. Zero on any axis is invalid.
    AABB3D bounds { .min = glm::vec3(-1.0F), .max = glm::vec3(1.0F) }; ///< Continuous extent subdivided.

    /** @brief Edge length of one cell along each axis. */
    [[nodiscard]] glm::vec3 cell_size() const noexcept
    {
        return bounds.extent() / glm::vec3(resolution);
    }

    /** @brief Total cell count. */
    [[nodiscard]] size_t cell_count() const noexcept
    {
        return static_cast<size_t>(resolution.x) * resolution.y * resolution.z;
    }

    /** @brief Corner count, one greater than the cell count on each axis. */
    [[nodiscard]] size_t corner_count() const noexcept
    {
        return static_cast<size_t>(resolution.x + 1U)
            * (resolution.y + 1U) * (resolution.z + 1U);
    }

    /**
     * @brief Linear index of a cell, x-major.
     * @param c Cell coordinate. Not bounds-checked.
     */
    [[nodiscard]] size_t index(const glm::uvec3& c) const noexcept
    {
        return (static_cast<size_t>(c.z) * resolution.y + c.y) * resolution.x + c.x;
    }

    /**
     * @brief Whether a cell coordinate lies inside the lattice.
     * @param c Cell coordinate.
     */
    [[nodiscard]] bool in_bounds(const glm::uvec3& c) const noexcept
    {
        return c.x < resolution.x && c.y < resolution.y && c.z < resolution.z;
    }

    /**
     * @brief World position of a cell's centre.
     * @param c Cell coordinate.
     */
    [[nodiscard]] glm::vec3 cell_center(const glm::uvec3& c) const noexcept
    {
        return bounds.min + (glm::vec3(c) + 0.5F) * cell_size();
    }

    /**
     * @brief World position of a lattice corner.
     * @param c Corner coordinate, valid up to resolution inclusive.
     */
    [[nodiscard]] glm::vec3 corner_position(const glm::uvec3& c) const noexcept
    {
        return bounds.min + glm::vec3(c) * cell_size();
    }

    /**
     * @brief Cell containing a world position, clamped to the lattice.
     * @param p World position. Points outside bounds clamp to the edge cell.
     */
    [[nodiscard]] glm::uvec3 cell_at(const glm::vec3& p) const noexcept
    {
        const glm::vec3 local = (p - bounds.min) / cell_size();
        const glm::vec3 clamped = glm::clamp(
            glm::floor(local), glm::vec3(0.0F), glm::vec3(resolution) - 1.0F);
        return glm::uvec3(clamped);
    }

    /**
     * @brief A lattice over the same bounds at a different resolution.
     * @param res New cell count per axis.
     *
     * Extraction resolution independent of simulation resolution is this
     * call.
     */
    [[nodiscard]] Lattice3D resampled(const glm::uvec3& res) const noexcept
    {
        return { .resolution = res, .bounds = bounds };
    }
};

/**
 * @struct Lattice2D
 * @brief A regular subdivision of an AABB2D into a cell count per axis.
 *
 * 2D sibling of Lattice3D. Pairs the continuous extent AABB2D describes
 * with a discrete resolution: quadrant subdivision is resolution {2, 2},
 * an NDC-space grid for hit testing or occupancy is any other
 * resolution. Everything else is derived: cell size, cell centre,
 * corner position, linear index.
 *
 * Indexing is x-major, y outermost, matching Lattice3D's convention
 * with the z axis simply absent rather than fixed at 1: a 2D lattice is
 * a distinct type from a 3D lattice with unit depth, not a special case
 * of it, since callers working purely in 2D should never need to reason
 * about a z coordinate that does not exist for their problem.
 */
struct Lattice2D {
    glm::uvec2 resolution { 1U }; ///< Cell count per axis. Zero on either axis is invalid.
    AABB2D bounds { .min = glm::vec2(-1.0F), .max = glm::vec2(1.0F) }; ///< Continuous extent subdivided.

    /** @brief Edge length of one cell along each axis. */
    [[nodiscard]] glm::vec2 cell_size() const noexcept
    {
        return glm::vec2(bounds.width(), bounds.height()) / glm::vec2(resolution);
    }

    /** @brief Total cell count. */
    [[nodiscard]] size_t cell_count() const noexcept
    {
        return static_cast<size_t>(resolution.x) * resolution.y;
    }

    /** @brief Corner count, one greater than the cell count on each axis. */
    [[nodiscard]] size_t corner_count() const noexcept
    {
        return static_cast<size_t>(resolution.x + 1U) * (resolution.y + 1U);
    }

    /**
     * @brief Linear index of a cell, x-major.
     * @param c Cell coordinate. Not bounds-checked.
     */
    [[nodiscard]] size_t index(const glm::uvec2& c) const noexcept
    {
        return static_cast<size_t>(c.y) * resolution.x + c.x;
    }

    /**
     * @brief Whether a cell coordinate lies inside the lattice.
     * @param c Cell coordinate.
     */
    [[nodiscard]] bool in_bounds(const glm::uvec2& c) const noexcept
    {
        return c.x < resolution.x && c.y < resolution.y;
    }

    /**
     * @brief Position of a cell's centre.
     * @param c Cell coordinate.
     */
    [[nodiscard]] glm::vec2 cell_center(const glm::uvec2& c) const noexcept
    {
        return bounds.min + (glm::vec2(c) + 0.5F) * cell_size();
    }

    /**
     * @brief Position of a lattice corner.
     * @param c Corner coordinate, valid up to resolution inclusive.
     */
    [[nodiscard]] glm::vec2 corner_position(const glm::uvec2& c) const noexcept
    {
        return bounds.min + glm::vec2(c) * cell_size();
    }

    /**
     * @brief Cell containing a position, clamped to the lattice.
     * @param p Position. Points outside bounds clamp to the edge cell.
     */
    [[nodiscard]] glm::uvec2 cell_at(const glm::vec2& p) const noexcept
    {
        const glm::vec2 local = (p - bounds.min) / cell_size();
        const glm::vec2 clamped = glm::clamp(
            glm::floor(local), glm::vec2(0.0F), glm::vec2(resolution) - 1.0F);
        return glm::uvec2(clamped);
    }

    /**
     * @brief A lattice over the same bounds at a different resolution.
     * @param res New cell count per axis.
     */
    [[nodiscard]] Lattice2D resampled(const glm::uvec2& res) const noexcept
    {
        return { .resolution = res, .bounds = bounds };
    }

    /**
     * @brief Quadrant lattice over NDC space, resolution {2, 2}.
     *
     * The specific case that motivated this type: NDC space split into
     * four quadrants by sign of x and y. cell_at({0.3, -0.6}) returns
     * {1, 0} (positive x, negative y quadrant).
     */
    [[nodiscard]] static Lattice2D ndc_quadrants() noexcept
    {
        return { .resolution = { 2U, 2U }, .bounds = { .min = glm::vec2(-1.0F), .max = glm::vec2(1.0F) } };
    }
};

} // namespace MayaFlux::Kinesis
