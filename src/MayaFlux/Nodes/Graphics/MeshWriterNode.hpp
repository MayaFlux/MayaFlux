#pragma once

#include "GeometryWriterNode.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "VertexSpec.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class MeshWriterNode
 * @brief Indexed triangle mesh for static or infrequently-updated geometry.
 *
 * Holds CPU-side MeshVertex and uint32_t index arrays.
 * Topology is always TRIANGLE_LIST. Index buffer is uploaded via
 * GeometryWriterNode::set_indices() on compute_frame().
 *
 * A mesh set from a MeshData also keeps what describes its parts: the submesh ranges (each
 * with its name, material and whatever else the producer attached) and the polygon faces the
 * triangles came from. They describe the arrays they arrived with, so whatever replaces the
 * vertices with another count or the indices drops them, and the arrays alone never have any.
 * Neither is drawn: they travel with the geometry to whatever reads it back.
 *
 * For GPU-deformed mesh data, populate m_vertices externally via
 * set_mesh() and let compute_frame() handle the upload on the next cycle.
 */
class MAYAFLUX_API MeshWriterNode : public GeometryWriterNode {
public:
    explicit MeshWriterNode(size_t initial_vertex_capacity = 1024);

    /**
     * @brief Replace all vertex and index data from a MeshData owner, keeping its submeshes
     *        and polygon faces.
     * @param data Source mesh; must be valid (MeshData::is_valid), so the vertex bytes and the
     *             triangle indices are not empty and any polygon faces fit the vertices.
     */
    void set_mesh(const Kakshya::MeshData& data);

    /**
     * @brief Replace all vertex and index data atomically. Submeshes and polygon faces are
     *        dropped, since they described the arrays that were replaced.
     * @param vertices Mesh vertices
     * @param indices  Triangle indices (must be a multiple of 3)
     */
    void set_mesh(
        std::span<const MeshVertex> vertices,
        std::span<const uint32_t> indices);

    /**
     * @brief Replace vertex array only; index array is unchanged.
     * @param vertices New vertex data. Submeshes and polygon faces are kept when the count is
     *        the same and dropped when it is not.
     */
    void set_mesh_vertices(std::span<const MeshVertex> vertices);

    /**
     * @brief Replace index array only; vertex array is unchanged.
     * @param indices New index data (must be a multiple of 3). Submeshes and polygon faces
     *        are dropped.
     */
    void set_mesh_indices(std::span<const uint32_t> indices);

    /**
     * @brief The node's current geometry as MeshData, the inverse of set_mesh(const MeshData&).
     *
     * Vertices and indices are copied as stored, in the node's own space with
     * no transform applied, with the submeshes and polygon faces the mesh was set with.
     * The canonical mesh layout is used.
     *
     * @return The mesh, or nullopt when the node has no vertices or no indices.
     */
    [[nodiscard]] std::optional<Kakshya::MeshData> get_mesh_data() const;

    [[nodiscard]] const std::vector<MeshVertex>& get_mesh_vertices() const { return m_vertices; }
    [[nodiscard]] const std::vector<uint32_t>& get_mesh_indices() const { return m_indices; }
    [[nodiscard]] size_t get_mesh_vertex_count() const { return m_vertices.size(); }
    [[nodiscard]] size_t get_mesh_face_count() const { return m_indices.size() / 3; }

    /** @brief The submesh ranges the mesh was set with, if it had any. */
    [[nodiscard]] const std::optional<Kakshya::RegionGroup>& get_mesh_submeshes() const { return m_submeshes; }

    /** @brief The polygon faces the mesh's triangles came from, if it had any. */
    [[nodiscard]] const std::optional<Kakshya::FaceList>& get_mesh_faces() const { return m_faces; }

    void clear_mesh();

    [[nodiscard]] Portal::Graphics::PrimitiveTopology get_primitive_topology() const override;

    void compute_frame() override;

private:
    std::vector<MeshVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::optional<Kakshya::RegionGroup> m_submeshes;
    std::optional<Kakshya::FaceList> m_faces;
    bool m_mesh_dirty { true };
};

} // namespace MayaFlux::Nodes::GpuSync
