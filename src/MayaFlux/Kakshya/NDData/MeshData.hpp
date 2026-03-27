#pragma once

#include "MeshAccess.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct MeshData
 * @brief Owning CPU-side representation of a loaded or generated mesh.
 *
 * MeshData is the named owner of the two DataVariant streams that a mesh
 * consists of: interleaved vertex bytes (vector<uint8_t>) and a flat
 * uint32_t index array (vector<uint32_t>). It is the handoff type between
 * IO (ModelReader) and GPU (MeshBuffer), and the input type for any future
 * mesh container (procedural, Yantra-generated, or deformed).
 *
 * Relationship to the accessor layer:
 * - MeshAccess is a non-owning view derived from MeshData via access().
 * - MeshInsertion writes into MeshData's two variants directly.
 * Neither accessor type changes when MeshData is introduced; they already
 * operate on DataVariant references regardless of ownership.
 *
 * Submesh structure follows the established Region convention:
 * a single RegionGroup named "submeshes" holds one Region per submesh,
 * each carrying index_start, index_count, vertex_offset, name, and
 * material_name as attributes.
 *
 * MeshData is not a container. It has no processing state, no read head,
 * no processor chain. For per-cycle mesh mutation use MeshWriterNode.
 * For offline or procedural generation feeding into MeshBuffer, construct
 * a MeshData and pass it in.
 */
struct MAYAFLUX_API MeshData {
    DataVariant vertex_variant; ///< vector<uint8_t>: interleaved vertex bytes
    DataVariant index_variant; ///< vector<uint32_t>: triangle index list
    VertexLayout layout;
    std::optional<RegionGroup> submeshes;

    // -------------------------------------------------------------------------
    // Construction helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Construct an empty MeshData with the canonical 60-byte mesh layout.
     *
     * Both variants are initialised to empty vectors of the correct type so
     * MeshInsertion can append into them without a type-check on first write.
     */
    static MeshData empty()
    {
        MeshData d;
        d.vertex_variant = std::vector<uint8_t> {};
        d.index_variant = std::vector<uint32_t> {};
        d.layout = VertexLayout::for_meshes(60);
        return d;
    }

    // -------------------------------------------------------------------------
    // Validity
    // -------------------------------------------------------------------------

    /**
     * @brief True when both variants are non-empty and layout stride is set.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
        const auto* vb = std::get_if<std::vector<uint8_t>>(&vertex_variant);
        const auto* ib = std::get_if<std::vector<uint32_t>>(&index_variant);
        return vb && !vb->empty()
            && ib && !ib->empty()
            && layout.stride_bytes > 0;
    }

    /**
     * @brief Number of vertices derived from vertex byte count and layout stride.
     * @return 0 if layout stride is zero or vertex_variant holds wrong type.
     */
    [[nodiscard]] uint32_t vertex_count() const noexcept
    {
        if (layout.stride_bytes == 0) {
            return 0;
        }
        const auto* vb = std::get_if<std::vector<uint8_t>>(&vertex_variant);
        if (!vb) {
            return 0;
        }
        return static_cast<uint32_t>(vb->size() / layout.stride_bytes);
    }

    /**
     * @brief Number of triangles (index count / 3).
     */
    [[nodiscard]] uint32_t face_count() const noexcept
    {
        const auto* ib = std::get_if<std::vector<uint32_t>>(&index_variant);
        if (!ib) {
            return 0;
        }
        return static_cast<uint32_t>(ib->size() / 3);
    }

    // -------------------------------------------------------------------------
    // Accessor layer
    // -------------------------------------------------------------------------

    /**
     * @brief Produce a non-owning MeshAccess view over this data.
     *
     * The returned MeshAccess borrows from this MeshData instance.
     * MeshData must outlive any MeshAccess derived from it.
     *
     * Returns std::nullopt if is_valid() is false.
     */
    [[nodiscard]] std::optional<MeshAccess> access() const
    {
        if (!is_valid()) {
            return std::nullopt;
        }
        return as_mesh_access(vertex_variant, index_variant, layout, submeshes);
    }
};

} // namespace MayaFlux::Kakshya
