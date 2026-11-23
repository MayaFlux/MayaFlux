#pragma once

#include "MeshAccess.hpp"

namespace MayaFlux::Kinesis {
struct TriangulationOptions;
}

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
 * each carrying the complete MeshSubrange as one typed value. A producer may
 * attach further structs or Facet values to the same Region, and they travel with
 * the submesh.
 *
 * The triangle list is always what draws. A mesh that came from polygons also keeps
 * them in faces, and index_variant is then the triangulation of those faces. A mesh
 * with no faces is triangles only, and every consumer that reads index_variant works
 * for both. face_count() counts triangles, polygon_count() counts the faces the mesh
 * was made of.
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
    std::optional<FaceList> faces; ///< The polygons index_variant was derived from, when there were any

    // -------------------------------------------------------------------------
    // Construction helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Construct an empty MeshData with the canonical 68-byte mesh layout.
     *
     * Both variants are initialised to empty vectors of the correct type so
     * MeshInsertion can append into them without a type-check on first write.
     */
    static MeshData empty()
    {
        MeshData d;
        d.vertex_variant = std::vector<uint8_t> {};
        d.index_variant = std::vector<uint32_t> {};
        d.layout = VertexLayout::for_meshes(68);
        return d;
    }

    // -------------------------------------------------------------------------
    // Validity
    // -------------------------------------------------------------------------

    /**
     * @brief True when both variants are non-empty, layout stride is set and any
     *        polygon faces are well formed over the vertices.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
        const auto* vb = std::get_if<std::vector<uint8_t>>(&vertex_variant);
        const auto* ib = std::get_if<std::vector<uint32_t>>(&index_variant);
        return vb && !vb->empty()
            && ib && !ib->empty()
            && layout.stride_bytes > 0
            && (!faces.has_value() || faces_are_valid());
    }

    /**
     * @brief True when every face has at least three corners, the sizes add up to the
     *        corners, and every corner names a vertex. A mesh without faces passes.
     */
    [[nodiscard]] bool faces_are_valid() const noexcept;

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

    /**
     * @brief True when the mesh keeps the polygon faces its triangles were derived from.
     */
    [[nodiscard]] bool has_polygons() const noexcept
    {
        return faces.has_value();
    }

    /**
     * @brief Number of faces the mesh was made of: its polygons, or its triangles when
     *        it has none.
     */
    [[nodiscard]] uint32_t polygon_count() const noexcept
    {
        return faces.has_value() ? static_cast<uint32_t>(faces->sizes.size()) : face_count();
    }

    // -------------------------------------------------------------------------
    // Polygons
    // -------------------------------------------------------------------------

    /**
     * @brief Make the mesh from polygon faces over its own vertices.
     *
     * Reads the positions from vertex_variant through layout, triangulates every face
     * (see Kinesis::triangulate_faces), keeps @p polygons in faces and replaces
     * index_variant with the triangles. Submeshes are dropped because their ranges
     * refer to the index array that was replaced. Nothing changes when this fails.
     *
     * A face that cannot be triangulated stays in faces and has no triangles.
     *
     * @param polygons Corners of every face over the vertices of vertex_variant.
     * @return The faces that could not be triangulated, which may be none, or nullopt when
     * the vertices hold no position attribute or the faces are not well formed over them.
     */
    [[nodiscard]] std::optional<std::vector<uint32_t>> assign_faces(FaceList polygons);

    /** @brief As above, with the choices a triangulation leaves open. */
    [[nodiscard]] std::optional<std::vector<uint32_t>> assign_faces(
        FaceList polygons, const Kinesis::TriangulationOptions& options);

    // -------------------------------------------------------------------------
    // Submeshes
    // -------------------------------------------------------------------------

    /**
     * @brief Take one submesh back out as a mesh of its own.
     *
     * Copies the submesh's vertices (MeshSubrange::vertex_offset and vertex_count) and
     * its triangles, with the indices made local to the copy. The result holds that one
     * submesh, as the Region it had here: its MeshSubrange now starts at index and vertex
     * zero, and every other struct or Facet value on the Region, and on the group, comes
     * along. The polygon faces are not carried, since they span the whole mesh, so the
     * result is triangles only.
     *
     * @param index Position of the submesh in the "submeshes" group.
     * @return nullopt when there is no such submesh, it has no MeshSubrange or no recorded
     * vertex range, its ranges leave the arrays, or a triangle names a vertex outside it.
     */
    [[nodiscard]] std::optional<MeshData> extract_submesh(size_t index) const;

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
        return as_mesh_access(vertex_variant, index_variant, layout, submeshes,
            faces.has_value() ? &*faces : nullptr);
    }
};

} // namespace MayaFlux::Kakshya
