#pragma once

#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class VertexInsertion
 * @brief Write counterpart to VertexAccess: splits interleaved vertex bytes into typed channels.
 *
 * Parallel to MeshInsertion and DataInsertion: holds a mutable reference to a
 * channel list and populates it. Each VertexLayout attribute becomes one
 * DataVariant, in layout order, holding the attribute for every vertex:
 *
 *   VERTEX_POSITIONS_3D, NORMALS_3D, TANGENTS_3D, COLORS_RGB -> vector<glm::vec3>
 *   TEXTURE_COORDS_2D                                        -> vector<glm::vec2>
 *   VERTEX_COLORS_RGBA                                       -> vector<glm::vec4>
 *   SCALAR_F32                                               -> vector<float>
 *
 * For the canonical point, line and mesh layouts the channel order matches the
 * slot order as_point_vertex_access() and its siblings consume, so a read
 * channel list can be written back without reordering.
 *
 * Nothing is written unless the layout and byte count are both valid.
 *
 * Usage:
 * @code
 * std::vector<DataVariant> channels;
 * VertexInsertion ins(channels);
 * if (ins.insert_interleaved(bytes, layout)) {
 *     auto& positions = std::get<std::vector<glm::vec3>>(channels[0]);
 * }
 * @endcode
 */
class MAYAFLUX_API VertexInsertion {
public:
    /**
     * @brief Construct with a mutable reference to the channel list.
     * @param channels Receives one typed DataVariant per layout attribute.
     */
    explicit VertexInsertion(std::vector<DataVariant>& channels);

    /**
     * @brief Whether a layout can be split into typed channels.
     *
     * Requires a nonzero stride, at least one attribute, a supported modality
     * per attribute, and every attribute lying within one stride.
     */
    [[nodiscard]] static bool can_decode(const VertexLayout& layout) noexcept;

    /**
     * @brief Replace the channel list with the attributes of interleaved vertices.
     *
     * The vertex count is derived from the byte count, not from
     * layout.vertex_count, so a layout describing capacity cannot over-read.
     *
     * @param vertex_bytes Interleaved vertices; size must be a nonzero multiple of layout.stride_bytes.
     * @param layout       Attribute offsets and stride of @p vertex_bytes.
     * @return False, with an error logged and the channel list untouched, when the layout or byte count is invalid.
     */
    bool insert_interleaved(
        std::span<const uint8_t> vertex_bytes,
        const VertexLayout& layout);

    /**
     * @brief Empty the channel list and forget the last layout.
     */
    void clear();

    /**
     * @brief Layout of the last successful insertion, with vertex_count set to the decoded count.
     */
    [[nodiscard]] const VertexLayout& layout() const noexcept { return m_layout; }

    /**
     * @brief Number of vertices decoded by the last successful insertion.
     */
    [[nodiscard]] uint32_t vertex_count() const noexcept { return m_layout.vertex_count; }

private:
    std::vector<DataVariant>& m_channels;
    VertexLayout m_layout;
};

} // namespace MayaFlux::Kakshya
