#pragma once

#include "SpatialData.hpp"
#include "VertexAccess.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Whole 68-byte vertex records of one spatial frame.
 *
 * Every vertex is built complete: position, color, scalar, uv, normal and tangent.
 * Attributes are matched by the names the spatial capture writes: "color" (vec3 or
 * vec4), "scalar" (float), "uv" (vec2), "normal" (vec3) and "tangent" (vec3 or
 * vec4). A Constant attribute gives every vertex its value, a Uniform one gives
 * every vertex of a curve its curve's value, and a Varying one is read per vertex.
 * What the frame does not carry comes from @p config. An attribute under another
 * name, of another type, or whose size does not match its scope is not a column of
 * the vertex and is left out.
 *
 * Curve frames use the line vertex defaults and layout, point frames the point ones.
 *
 * @param frame  Frame to read. Its positions give the vertex count.
 * @param config Values for every column the frame does not carry.
 * @return The records and their layout, or nullopt for a frame with no positions.
 */
[[nodiscard]] MAYAFLUX_API std::optional<VertexAccess> as_spatial_vertex_access(
    const SpatialData& frame,
    const VertexAccessConfig& config = {});

/**
 * @brief The cluster id of every vertex of a spatial frame.
 *
 * Read from the attribute named "cluster" (uint32, any scope), the one the spatial
 * capture writes from a buffer's cluster ids. A frame without a usable one gives
 * every vertex cluster 0.
 *
 * @return One id per position.
 */
[[nodiscard]] MAYAFLUX_API std::vector<uint32_t> spatial_cluster_ids(const SpatialData& frame);

} // namespace MayaFlux::Kakshya
