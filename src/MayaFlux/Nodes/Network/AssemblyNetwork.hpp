#pragma once

#include "NodeNetwork.hpp"
#include "Operators/AssemblyOperator.hpp"

namespace MayaFlux::Nodes::Network {

/**
 * @class AssemblyNetwork
 * @brief Holds hundreds or thousands of independent geometry items with no
 *        shared topology, layout, or relation to each other, rendered as
 *        one milled draw.
 *
 * Thin wrapper around a single AssemblyOperator, forwarding add_geometry()/
 * remove_geometry() so the common case doesn't require reaching into
 * create_operator<AssemblyOperator>() manually. Not a specialized operator
 * (PathOperator/TopologyOperator/PhysicsOperator interpolate, connect, or
 * simulate; this does none of that), and not built-in-primitive convenience:
 * callers supply their own geometry from Kinesis generators, input, or
 * procedural code and this just holds and draws it.
 *
 * Renders through the standard NetworkGeometryBuffer(network)->setup_rendering
 * path with @c triangulate enabled: milling erases the per-item layout/
 * topology mismatch, so a heterogeneous, arbitrarily large item count draws
 * through exactly one RenderProcessor pass, the same way a mixed-topology
 * PathOperator does.
 */
class MAYAFLUX_API AssemblyNetwork : public NodeNetwork {
public:
    AssemblyNetwork();

    void process_batch(unsigned int num_samples) override;
    [[nodiscard]] size_t get_node_count() const override;

    NetworkOperator* get_operator() override { return m_operator.get(); }
    const NetworkOperator* get_operator() const override { return m_operator.get(); }
    bool has_operator() const override { return m_operator != nullptr; }

    [[nodiscard]] AssemblyOperator* assembly() const { return m_operator.get(); }

    std::shared_ptr<GpuSync::GeometryLeafNode> add_geometry(
        std::vector<glm::vec3> positions,
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST)
    {
        return m_operator->add_geometry(std::move(positions), topology);
    }

    std::shared_ptr<GpuSync::GeometryLeafNode> add_geometry(
        std::vector<Kakshya::Vertex> vertices,
        Portal::Graphics::PrimitiveTopology topology = Portal::Graphics::PrimitiveTopology::POINT_LIST)
    {
        return m_operator->add_geometry(std::move(vertices), topology);
    }

    std::shared_ptr<GpuSync::MeshWriterNode> add_geometry(const Kakshya::MeshData& mesh)
    {
        return m_operator->add_geometry(mesh);
    }

    void add_geometry(std::shared_ptr<GpuSync::GeometryWriterNode> node)
    {
        m_operator->add_geometry(std::move(node));
    }

    bool remove_geometry(const std::shared_ptr<GpuSync::GeometryWriterNode>& node)
    {
        return m_operator->remove_geometry(node);
    }

private:
    std::unique_ptr<AssemblyOperator> m_operator;
};

} // namespace MayaFlux::Nodes::Network
