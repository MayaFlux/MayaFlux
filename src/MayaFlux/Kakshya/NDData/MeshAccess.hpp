#pragma once

#include "MayaFlux/Transitive/Reflect/Describe.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Transitive/Facet/Facets.hpp"
#include "FaceList.hpp"
#include "VertexLayout.hpp"

namespace MayaFlux::Kakshya {

/**
 * @struct MeshSubrange
 * @brief Where one submesh lives in the shared index and vertex arrays, and its names.
 *
 * index_start and index_count are element indices into the uint32_t index array, not
 * byte offsets. Callers multiply by sizeof(uint32_t) when computing GPU offsets.
 *
 * vertex_offset and vertex_count are the submesh's own vertices within the shared vertex
 * array, in vertices rather than bytes: it owns [vertex_offset, vertex_offset + vertex_count).
 * The indices in its range already address the shared array, so vertex_offset is not added
 * to them. It is what lets a submesh be taken back out on its own (MeshData::extract_submesh)
 * and what a draw call needs as a base vertex when it is drawn with indices local to it. A
 * vertex_count of zero means the vertex range was not recorded.
 *
 * name and material_name are sourced from the originating file; both may be
 * empty for single-mesh files or loaders that do not expose names.
 *
 * The Region that holds a MeshSubrange may carry any further structs or Facet values a
 * producer attaches, and MeshInsertion keeps them when it places the submesh.
 */
struct MAYAFLUX_API MeshSubrange {
    uint32_t index_start {};
    uint32_t index_count {};
    uint32_t vertex_offset {};
    uint32_t vertex_count {};
    std::string name;
    std::string material_name;
    std::string diffuse_path;
    bool diffuse_embedded {};

    /**
     * @brief Convert this subrange to a Region for use in RegionGroup.
     *
     * Coordinates are [index_start, index_start + index_count).
     * The complete subrange is stored as one typed Facet value.
     */
    [[nodiscard]] Region to_region() const;
};

MF_DESCRIBE(MeshSubrange)

/**
 * @struct MeshAccess
 * @brief Non-owning read view over a pair of DataVariant streams representing
 *        interleaved vertex bytes and a flat uint32_t index array.
 *
 * Parallel to VertexAccess and TextureAccess: describes what the data IS
 * in memory terms with no knowledge of Vulkan, Portal, or concrete vertex
 * structs (MeshVertex). The layout field carries full semantic description
 * via VertexLayout::for_meshes(), including stride and attribute offsets.
 *
 * Ownership contract: the two DataVariant instances and the optional
 * RegionGroup must outlive this struct. MeshAccess holds raw pointers and
 * spans into their storage.
 *
 * Zero-copy: both vertex_ptr and index_ptr point directly into the storage
 * of their respective DataVariant. No conversion buffer is needed because
 * MeshInsertion always writes the canonical 68-byte interleaved format before
 * producing a MeshAccess.
 *
 * Submesh structure: when the source file contains multiple meshes, submeshes
 * holds a RegionGroup named "submeshes". Each Region maps to one MeshSubrange
 * and carries the complete MeshSubrange as a typed Facet value. Single-mesh
 * files leave submeshes as nullopt; the entire index buffer is one contiguous
 * range.
 */
struct MAYAFLUX_API MeshAccess {
    const void* vertex_ptr = nullptr;
    size_t vertex_bytes = 0;

    const uint32_t* index_ptr = nullptr;
    uint32_t index_count = 0;

    VertexLayout layout;

    std::optional<RegionGroup> submeshes;

    /**
     * Polygon faces the triangle indices were derived from, or null when the mesh is
     * triangles only. Borrowed from the owner, which must outlive this view.
     */
    const FaceList* faces = nullptr;

    // -------------------------------------------------------------------------
    // Convenience queries
    // -------------------------------------------------------------------------

    /**
     * @brief Number of vertices derived from vertex_bytes and layout stride.
     * @return 0 if layout.stride_bytes is zero.
     */
    [[nodiscard]] uint32_t vertex_count() const noexcept
    {
        if (layout.stride_bytes == 0) {
            return 0;
        }
        return static_cast<uint32_t>(vertex_bytes / layout.stride_bytes);
    }

    /**
     * @brief Number of triangles (index_count / 3).
     */
    [[nodiscard]] uint32_t face_count() const noexcept
    {
        return index_count / 3;
    }

    /**
     * @brief True when the mesh keeps the polygon faces its triangles came from.
     */
    [[nodiscard]] bool has_polygons() const noexcept
    {
        return faces != nullptr;
    }

    /**
     * @brief True when both vertex and index data are non-empty.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
        return vertex_ptr != nullptr && vertex_bytes > 0
            && index_ptr != nullptr && index_count > 0
            && layout.stride_bytes > 0;
    }

    /**
     * @brief True when submesh region data is present.
     */
    [[nodiscard]] bool has_submeshes() const noexcept
    {
        return submeshes.has_value() && !submeshes->regions.empty();
    }

    /**
     * @brief Span over the raw index array.
     */
    [[nodiscard]] std::span<const uint32_t> indices() const noexcept
    {
        return { index_ptr, index_count };
    }

    /**
     * @brief Span over the raw vertex bytes.
     */
    [[nodiscard]] std::span<const std::byte> vertices() const noexcept
    {
        return { static_cast<const std::byte*>(vertex_ptr), vertex_bytes };
    }
};

/**
 * @brief Construct a MeshAccess from two DataVariant instances.
 *
 * vertex_variant must hold vector<uint8_t> (canonical interleaved bytes).
 * index_variant  must hold vector<uint32_t>.
 * layout must have stride_bytes > 0 and at least one attribute.
 * submeshes is forwarded as-is; pass std::nullopt for single-mesh data.
 *
 * Returns std::nullopt and logs an error for any type mismatch.
 *
 * The RegionGroup convention: a single RegionGroup named "submeshes" holds
 * one Region per submesh. Each Region carries coordinates
 * [index_start, index_start + index_count) in the index dimension and
 * stores a typed MeshSubrange value. This mirrors the
 * audio/video pattern where e.g. "transients" is the group and each onset
 * event is a Region inside it.
 *
 * faces is stored in the view as given and is not checked here.
 */
[[nodiscard]] MAYAFLUX_API std::optional<MeshAccess> as_mesh_access(
    const DataVariant& vertex_variant,
    const DataVariant& index_variant,
    const VertexLayout& layout,
    std::optional<RegionGroup> submeshes = std::nullopt,
    const FaceList* faces = nullptr);

} // namespace MayaFlux::Kakshya
#include "MeshAccess.describe.hpp"
