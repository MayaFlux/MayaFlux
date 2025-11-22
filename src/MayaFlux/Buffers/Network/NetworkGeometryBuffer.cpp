#include "NetworkGeometryBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/PointNode.hpp"

namespace MayaFlux::Buffers {

NetworkGeometryBuffer::NetworkGeometryBuffer(
    std::shared_ptr<Nodes::NodeNetwork> network,
    const std::string& binding_name,
    float over_allocate_factor)
    : VKBuffer(
          calculate_buffer_size(network, over_allocate_factor),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_network(std::move(network))
    , m_binding_name(binding_name)
{
    if (!m_network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot create NetworkGeometryBuffer with null NodeNetwork");
    }

    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created NetworkGeometryBuffer '{}' for {} nodes ({} bytes estimated)",
        m_binding_name,
        m_network->get_node_count(),
        get_size_bytes());
}

void NetworkGeometryBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<NetworkGeometryBuffer>(shared_from_this());

    m_processor = std::make_shared<NetworkGeometryProcessor>();
    m_processor->set_processing_token(token);
    m_processor->bind_network(
        m_binding_name,
        m_network,
        self);

    set_default_processor(m_processor);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Setup NetworkGeometryProcessor for '{}' with token {}",
        m_binding_name, static_cast<int>(token));
}

uint32_t NetworkGeometryBuffer::get_vertex_count() const
{
    // Each node in network typically produces 1 vertex (for PointNode networks)
    // Processor will provide actual count after aggregation
    return static_cast<uint32_t>(m_network ? m_network->get_node_count() : 0);
}

size_t NetworkGeometryBuffer::calculate_buffer_size(
    const std::shared_ptr<Nodes::NodeNetwork>& network,
    float over_allocate_factor)
{
    if (!network) {
        return 0;
    }

    size_t node_count = network->get_node_count();

    if (node_count == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "NodeNetwork has zero nodes. Buffer will be created with minimum size.");
        return 4096;
    }

    // Estimate: assume each node produces one PointVertex (position + color + size)
    size_t vertex_size = sizeof(Nodes::GpuSync::PointVertex);
    size_t base_size = node_count * vertex_size;

    size_t allocated_size = static_cast<size_t>(
        static_cast<float>(base_size) * over_allocate_factor);

    if (over_allocate_factor > 1.0f) {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferManagement,
            "Over-allocated network geometry buffer: {} nodes × {} bytes = {} → {} bytes ({}x)",
            node_count, vertex_size, base_size, allocated_size, over_allocate_factor);
    }

    return allocated_size;
}

} // namespace MayaFlux::Buffers
