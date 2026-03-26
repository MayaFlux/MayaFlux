#pragma once

#include "GeometryWriterNode.hpp"
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
 * For GPU-deformed mesh data, populate m_vertices externally via
 * set_mesh() and let compute_frame() handle the upload on the next cycle.
 */
class MAYAFLUX_API MeshWriterNode : public GeometryWriterNode {
public:
    explicit MeshWriterNode(size_t initial_vertex_capacity = 1024);

    /**
     * @brief Replace all vertex and index data atomically.
     * @param vertices Mesh vertices
     * @param indices  Triangle indices (must be a multiple of 3)
     */
    void set_mesh(
        std::span<const MeshVertex> vertices,
        std::span<const uint32_t> indices);

    /**
     * @brief Replace vertex array only; index array is unchanged.
     * @param vertices New vertex data
     */
    void set_mesh_vertices(std::span<const MeshVertex> vertices);

    /**
     * @brief Replace index array only; vertex array is unchanged.
     * @param indices New index data (must be a multiple of 3)
     */
    void set_mesh_indices(std::span<const uint32_t> indices);

    [[nodiscard]] const std::vector<MeshVertex>& get_mesh_vertices() const { return m_vertices; }
    [[nodiscard]] const std::vector<uint32_t>& get_mesh_indices() const { return m_indices; }
    [[nodiscard]] size_t get_mesh_vertex_count() const { return m_vertices.size(); }
    [[nodiscard]] size_t get_mesh_face_count() const { return m_indices.size() / 3; }

    void clear_mesh();

    [[nodiscard]] Portal::Graphics::PrimitiveTopology get_primitive_topology() const override;

    void compute_frame() override;

private:
    std::vector<MeshVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};

} // namespace MayaFlux::Nodes::GpuSync
