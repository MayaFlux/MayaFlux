#pragma once

#include "MeshAccess.hpp"
#include "VertexFormats.hpp"

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
    // Construction from arrays
    // -------------------------------------------------------------------------

    /**
     * @brief A mesh over interleaved records and the layout that describes them.
     *
     * The records are copied as they are, whatever the stride, so every attribute the
     * layout names comes through untouched.
     *
     * This and every from_* below check what they are given, log why they refused, and
     * return nullopt rather than a mesh that would not be valid. Triangles are three vertex
     * indices each. Polygons are a FaceList, triangulated as assign_faces does: a face that
     * cannot be triangulated is kept in faces, has no triangles, and is logged. An attribute
     * a source does not supply keeps the MeshVertex default, so normals are (0, 0, 1) until
     * something derives them (Kinesis::write_vertex_normals).
     */
    [[nodiscard]] static std::optional<MeshData> from_records(
        std::span<const uint8_t> records,
        const VertexLayout& layout,
        std::span<const uint32_t> triangles);

    /**
     * @brief As above, from polygons. The layout needs a position attribute.
     *
     * Every polygon overload takes the two choices of Kinesis::triangulate_faces: whether a
     * convex quadrilateral splits along its shorter diagonal, and the convexity tolerance.
     */
    [[nodiscard]] static std::optional<MeshData> from_records(
        std::span<const uint8_t> records,
        const VertexLayout& layout,
        FaceList polygons,
        bool shorter_quad_diagonal = true,
        float convexity_tolerance = 1.0e-6F);

    /**
     * @brief A mesh over bare positions, every other attribute at its default.
     */
    [[nodiscard]] static std::optional<MeshData> from_positions(
        std::span<const glm::vec3> positions,
        std::span<const uint32_t> triangles);

    /** @brief As above, from polygons. */
    [[nodiscard]] static std::optional<MeshData> from_positions(
        std::span<const glm::vec3> positions,
        FaceList polygons,
        bool shorter_quad_diagonal = true,
        float convexity_tolerance = 1.0e-6F);

    /**
     * @brief A mesh over arrays of Vertex, PointVertex, LineVertex or MeshVertex.
     *
     * The four share one record layout, so their bytes are used directly and every field,
     * uv included, is kept. Any contiguous range of them will do: a vector, an array or a
     * span.
     */
    template <std::ranges::contiguous_range R>
        requires CanonicalVertex<std::ranges::range_value_t<R>>
    [[nodiscard]] static std::optional<MeshData> from_vertices(
        const R& vertices, std::span<const uint32_t> triangles)
    {
        return from_records(canonical_bytes(vertices), VertexLayout::for_meshes(68), triangles);
    }

    /** @brief As above, from polygons. */
    template <std::ranges::contiguous_range R>
        requires CanonicalVertex<std::ranges::range_value_t<R>>
    [[nodiscard]] static std::optional<MeshData> from_vertices(
        const R& vertices,
        FaceList polygons,
        bool shorter_quad_diagonal = true,
        float convexity_tolerance = 1.0e-6F)
    {
        return from_records(canonical_bytes(vertices), VertexLayout::for_meshes(68), std::move(polygons),
            shorter_quad_diagonal, convexity_tolerance);
    }

    /**
     * @brief A mesh over the arrays a DataVariant can hold.
     *
     * @p vertices may be:
     * - vector<glm::vec3>, the positions;
     * - vector<float> or vector<double>, positions as flat x, y, z triples;
     * - vector<uint8_t>, interleaved records laid out as @p layout says.
     *
     * @p triangles may be vector<uint32_t>, vector<uint16_t> or vector<uint8_t>, widened
     * to 32 bits. Anything else is refused, and so is a flat array that is not a whole
     * number of triples.
     *
     * @param layout Used only for a vector<uint8_t> of records.
     */
    [[nodiscard]] static std::optional<MeshData> from_variants(
        const DataVariant& vertices,
        const DataVariant& triangles,
        const VertexLayout& layout = VertexLayout::for_meshes(68));

    /** @brief As above, from polygons. */
    [[nodiscard]] static std::optional<MeshData> from_variants(
        const DataVariant& vertices,
        FaceList polygons,
        bool shorter_quad_diagonal = true,
        float convexity_tolerance = 1.0e-6F,
        const VertexLayout& layout = VertexLayout::for_meshes(68));

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
     * @param shorter_quad_diagonal Split a convex quadrilateral along its shorter diagonal;
     * false splits from the first corner to the third.
     * @param convexity_tolerance As Kinesis::polygon_is_convex.
     * @return The faces that could not be triangulated, which may be none, or nullopt when
     * the vertices hold no position attribute or the faces are not well formed over them.
     */
    [[nodiscard]] std::optional<std::vector<uint32_t>> assign_faces(
        FaceList polygons, bool shorter_quad_diagonal = true, float convexity_tolerance = 1.0e-6F);

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

private:
    template <std::ranges::contiguous_range R>
    [[nodiscard]] static std::span<const uint8_t> canonical_bytes(const R& vertices) noexcept
    {
        return std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(std::ranges::data(vertices)),
            std::ranges::size(vertices) * sizeof(std::ranges::range_value_t<R>));
    }
};

} // namespace MayaFlux::Kakshya
