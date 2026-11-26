#pragma once

#include "Incidence.hpp"

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @struct Identification
 * @brief Corners grouped into the vertices they will share.
 *
 * New vertices are numbered by the vertex they came from, then by the order in which
 * their first corner appears. A vertex no corner names has no new vertex.
 */
struct Identification {
    std::vector<uint32_t> corner_vertex; ///< The new vertex of every corner.
    std::vector<uint32_t> source_vertex; ///< The old vertex each new vertex came from.
};

namespace detail {

    /**
     * @brief Group the corners of each vertex by the equivalence @p same.
     *
     * @p same takes two corner indices and answers whether they agree. It must be
     * reflexive, symmetric and transitive, and safe to run unsequenced.
     */
    template <typename Same>
    Identification identify_with(const Incidence& incidence, const Same& same)
    {
        const VertexCorners around = incidence.vertex_corners();
        const size_t vertices = incidence.vertex_count();

        std::vector<uint32_t> group(incidence.corner_count());
        std::vector<uint32_t> bases(vertices + 1, 0U);

        for_each_indexed(std::execution::par_unseq, std::span<uint32_t>(bases).subspan(1),
            [&](size_t vertex, uint32_t& groups) {
                const auto list = around.around(vertex);
                uint32_t next = 0;
                for (size_t i = 0; i < list.size(); ++i) {
                    uint32_t assigned = next;
                    for (size_t j = 0; j < i; ++j) {
                        if (same(list[j], list[i])) {
                            assigned = group[list[j]];
                            break;
                        }
                    }
                    group[list[i]] = assigned;
                    if (assigned == next) {
                        ++next;
                    }
                }
                groups = next;
            });

        std::partial_sum(bases.begin(), bases.end(), bases.begin());

        Identification result;
        result.corner_vertex.resize(incidence.corner_count());
        result.source_vertex.resize(bases.back());

        for_each_indexed(std::execution::par_unseq, std::span<const uint32_t>(bases).first(vertices),
            [&](size_t vertex, const uint32_t& base) {
                const uint32_t groups = bases[vertex + 1] - base;
                for (const uint32_t corner : around.around(vertex)) {
                    result.corner_vertex[corner] = base + group[corner];
                }
                for (uint32_t g = 0; g < groups; ++g) {
                    result.source_vertex[static_cast<size_t>(base) + g] = static_cast<uint32_t>(vertex);
                }
            });

        return result;
    }

} // namespace detail

/**
 * @brief Give every distinct combination of attributes at a vertex its own vertex.
 *
 * Corners that name the same vertex stay together when every channel agrees exactly
 * between them, and split apart otherwise. This is how attributes defined per corner
 * (a uv seam, a hard edge) become per vertex data. Channels compare with ==, so -0 and 0
 * agree. With no channel, every vertex stays whole.
 *
 * Work per vertex is quadratic in the number of distinct groups at it, which is small
 * wherever corners mostly agree. Vertices run in parallel.
 *
 * @param channels Spans of one value per corner.
 * @return nullopt when a channel does not have one value per corner.
 */
template <typename... T>
    requires(std::equality_comparable<T> && ...)
[[nodiscard]] std::optional<Identification> identify_corners(
    const Incidence& incidence, std::span<const T>... channels)
{
    if (((channels.size() != incidence.corner_count()) || ...)) {
        return std::nullopt;
    }
    const auto same = [&channels...](uint32_t a, uint32_t b) {
        return ((channels[a] == channels[b]) && ...);
    };
    return detail::identify_with(incidence, same);
}

/**
 * @struct Coincidence
 * @brief Points merged into the vertices they coincide with.
 */
struct Coincidence {
    std::vector<uint32_t> vertex_map; ///< The merged vertex of every point.
    std::vector<uint32_t> source_vertex; ///< The lowest numbered point of each merged vertex.
};

/**
 * @brief Merge points that lie within @p tolerance of one another.
 *
 * Closeness chains: if a is near b and b is near c, all three merge. A tolerance of zero
 * merges only equal positions. Merged vertices are numbered by their lowest point, in
 * order. A point that is not finite merges with nothing.
 *
 * Points are binned on a grid of cell size @p tolerance and sorted in parallel, then each
 * point looks only at its own cell and half of the neighbouring cells. Work is quadratic
 * in the number of points sharing a cell, so a tolerance far larger than the spacing of
 * the points is slow.
 *
 * @return nullopt for a tolerance that is negative or not finite.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Coincidence> merge_coincident_positions(
    std::span<const glm::vec3> positions, float tolerance);

} // namespace MayaFlux::Kinesis
