#pragma once

#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @brief Put interleaved vertices through a transform, in place.
 *
 * Positions take the matrix. Normals take its inverse transpose and are renormalised, so they
 * stay perpendicular under non-uniform scale. A 3D tangent takes the matrix's linear part and
 * is renormalised, and a 4D tangent does the same to xyz and keeps w. Every other attribute is
 * left alone. When the matrix has no inverse, only positions are placed.
 *
 * @param vertex_bytes Interleaved vertices; the size must be a nonzero multiple of layout.stride_bytes.
 * @param layout       Attribute offsets and stride of @p vertex_bytes. Needs a VERTEX_POSITIONS_3D attribute.
 * @param transform    Matrix to apply.
 * @return False, with the bytes untouched, when the layout or byte count is invalid.
 */
MAYAFLUX_API bool place_vertices(
    std::span<uint8_t> vertex_bytes,
    const VertexLayout& layout,
    const glm::mat4& transform) noexcept;

} // namespace MayaFlux::Kakshya
