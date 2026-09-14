#pragma once

#include "GeometryWriterNode.hpp"
#include "VertexSpec.hpp"

namespace MayaFlux::Nodes::GpuSync {

/**
 * @class GeometryLeafNode
 * @brief One independent, arbitrary-topology geometry item.
 *
 * Concrete GeometryWriterNode leaf holding a flat Vertex array, with no
 * interpolation, connectivity, or simulation of its own: callers supply
 * finished geometry (Kinesis generators, input, procedural code) and this
 * just holds and uploads it. Topology comes from the inherited
 * set_primitive_topology()/get_primitive_topology() (default POINT_LIST);
 * the reported vertex layout tracks whichever topology is current, since
 * Vertex/PointVertex/LineVertex/MeshVertex share one 60-byte offset table
 * and differ only in the scalar field's name.
 *
 * Intended to be held in bulk by AssemblyOperator, alongside other
 * GeometryWriterNode subclasses, with no shared layout or relation assumed
 * between items.
 */
class MAYAFLUX_API GeometryLeafNode : public GeometryWriterNode {
public:
    explicit GeometryLeafNode(size_t initial_capacity = 64);
    explicit GeometryLeafNode(std::vector<Vertex> vertices);
    explicit GeometryLeafNode(std::vector<glm::vec3> positions);

    /// @brief Replace geometry with plain positions (color/scalar/normal/uv default).
    void set_positions(std::vector<glm::vec3> positions);

    /// @brief Replace geometry with fully specified vertices.
    void set_geometry(std::vector<Vertex> vertices);

    void update_vertex(size_t index, const Vertex& vertex);

    [[nodiscard]] const std::vector<Vertex>& get_geometry() const { return m_items; }

    /**
     * @brief Mutable access for in-place edits. Does not itself mark the
     *        node dirty -- call set_geometry() (even with the same vector)
     *        or update_vertex() afterward so compute_frame() re-uploads.
     */
    [[nodiscard]] std::vector<Vertex>& get_geometry() { return m_items; }

    void clear_geometry();

    [[nodiscard]] size_t size() const { return m_items.size(); }

    /**
     * @brief No-op unless a mutator has run since the last call, so
     *        AssemblyOperator::process() calling this on every item every
     *        frame stays cheap regardless of how many items exist.
     */
    void compute_frame() override;

private:
    std::vector<Vertex> m_items;
    bool m_geometry_dirty { true };
};

} // namespace MayaFlux::Nodes::GpuSync
