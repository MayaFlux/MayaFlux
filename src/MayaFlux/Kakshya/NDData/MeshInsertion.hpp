#pragma once

#include "MeshAccess.hpp"
#include "NDData.hpp"
#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class MeshInsertion
 * @brief Write counterpart to MeshAccess.
 *
 * Parallel to EigenInsertion and DataInsertion: holds mutable references to
 * two DataVariant targets and populates them with interleaved vertex bytes and
 * flat uint32_t index data respectively. Callers supply typed spans; the class
 * handles memcpy into the canonical storage and constructs the VertexLayout.
 *
 * Submesh structure is recorded as a RegionGroup whose Regions map to the
 * index sub-ranges of each submitted batch. Callers accumulate batches via
 * insert_submesh() and then call build() to obtain a MeshAccess.
 *
 * The vertex DataVariant always receives vector<uint8_t> (raw bytes).
 * The index DataVariant always receives vector<uint32_t>.
 * Neither variant is touched until insert_submesh() or insert_flat() is called.
 *
 * Usage (single mesh, no submeshes):
 * @code
 * DataVariant vbuf, ibuf;
 * MeshInsertion ins(vbuf, ibuf);
 * ins.insert_flat(
 *     std::span<const uint8_t>{ raw_bytes, byte_count },
 *     std::span<const uint32_t>{ indices, index_count },
 *     VertexLayout::for_meshes(60));
 * auto access = ins.build();
 * @endcode
 *
 * Usage (multi-submesh, e.g. from assimp):
 * @code
 * DataVariant vbuf, ibuf;
 * MeshInsertion ins(vbuf, ibuf);
 * for (auto& sub : model.meshes) {
 *     ins.insert_submesh(sub.vertex_bytes, sub.indices,
 *                        sub.name, sub.material_name);
 * }
 * auto access = ins.build();
 * @endcode
 */
class MAYAFLUX_API MeshInsertion {
public:
    /**
     * @brief Construct with mutable references to the two storage variants.
     * @param vertex_variant Receives vector<uint8_t> of interleaved vertex data.
     * @param index_variant  Receives vector<uint32_t> of triangle indices.
     */
    MeshInsertion(DataVariant& vertex_variant, DataVariant& index_variant);

    // -------------------------------------------------------------------------
    // Write operations
    // -------------------------------------------------------------------------

    /**
     * @brief Insert a single flat mesh (no submesh tracking).
     *
     * Replaces any existing content in both variants. layout must have
     * stride_bytes > 0. vertex_bytes.size() must be a multiple of
     * layout.stride_bytes. index_data.size() must be a multiple of 3.
     *
     * @param vertex_bytes  Raw interleaved vertex data.
     * @param index_data    Triangle index list.
     * @param layout        Vertex attribute layout; stride_bytes must be set.
     */
    void insert_flat(
        std::span<const uint8_t> vertex_bytes,
        std::span<const uint32_t> index_data,
        const VertexLayout& layout);

    /**
     * @brief Append one submesh batch, accumulating into the shared buffers.
     *
     * Vertex bytes are appended to the existing vector<uint8_t>; indices are
     * offset by the current vertex count and appended to vector<uint32_t>.
     * A Region is added to the internal submesh RegionGroup recording the
     * index sub-range and names.
     *
     * layout is taken from the first call; subsequent calls must have the
     * same stride_bytes and attribute count. Mismatches log an error and
     * skip the batch.
     *
     * @param vertex_bytes   Raw interleaved vertex data for this submesh.
     * @param index_data     Triangle indices local to this submesh (0-based).
     * @param name           Submesh name (may be empty).
     * @param material_name  Material name (may be empty).
     * @param layout         Vertex layout; ignored after first call.
     */
    void insert_submesh(
        std::span<const uint8_t> vertex_bytes,
        std::span<const uint32_t> index_data,
        std::string_view name = {},
        std::string_view material_name = {},
        const VertexLayout& layout = VertexLayout::for_meshes(60));

    /**
     * @brief Clear both variants and reset all internal state.
     */
    void clear();

    // -------------------------------------------------------------------------
    // Finalise
    // -------------------------------------------------------------------------

    /**
     * @brief Produce a MeshAccess over the current variant contents.
     *
     * Returns std::nullopt if no data has been inserted or if either variant
     * holds the wrong type (indicates a programming error, logged as error).
     * The returned MeshAccess borrows from the two DataVariant references held
     * by this object; both must remain alive while the MeshAccess is in use.
     */
    [[nodiscard]] std::optional<MeshAccess> build() const;

private:
    DataVariant& m_vertex_variant;
    DataVariant& m_index_variant;

    VertexLayout m_layout;
    bool m_layout_set = false;
    uint32_t m_vertex_count = 0; ///< Running total across all submitted batches
    uint32_t m_index_count = 0;

    std::optional<RegionGroup> m_submeshes;

    void ensure_vertex_storage();
    void ensure_index_storage();

    [[nodiscard]] bool validate_layout(const VertexLayout& incoming) const;
};

} // namespace MayaFlux::Kakshya
