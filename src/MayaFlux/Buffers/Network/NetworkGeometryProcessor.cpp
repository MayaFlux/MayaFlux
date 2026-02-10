#include "NetworkGeometryProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Network/NodeNetwork.hpp"
#include "MayaFlux/Nodes/Network/Operators/GraphicsOperator.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NetworkGeometryProcessor::NetworkGeometryProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void NetworkGeometryProcessor::bind_network(
    const std::string& name,
    const std::shared_ptr<Nodes::Network::NodeNetwork>& network,
    const std::shared_ptr<VKBuffer>& vertex_buffer)
{
    if (!network) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind null network '{}'", name);
    }

    if (!vertex_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind network '{}' to null vertex buffer", name);
    }

    std::shared_ptr<VKBuffer> staging = nullptr;
    if (!vertex_buffer->is_host_visible()) {
        staging = create_staging_buffer(vertex_buffer->get_size_bytes());

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created staging buffer for device-local network geometry '{}' ({} bytes)",
            name, vertex_buffer->get_size_bytes());
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No staging needed for host-visible network geometry '{}'", name);
    }

    m_bindings[name] = NetworkBinding {
        .network = network,
        .gpu_vertex_buffer = vertex_buffer,
        .staging_buffer = staging
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound network '{}' ({} nodes, {} bytes buffer)",
        name, network->get_node_count(), vertex_buffer->get_size_bytes());
}

void NetworkGeometryProcessor::unbind_network(const std::string& name)
{
    if (m_bindings.erase(name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to unbind non-existent network '{}'", name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound network '{}'", name);
    }
}

bool NetworkGeometryProcessor::has_binding(const std::string& name) const
{
    return m_bindings.contains(name);
}

std::vector<std::string> NetworkGeometryProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

size_t NetworkGeometryProcessor::get_binding_count() const
{
    return m_bindings.size();
}

std::optional<NetworkGeometryProcessor::NetworkBinding>
NetworkGeometryProcessor::get_binding(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second;
    }
    return std::nullopt;
}

void NetworkGeometryProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_bindings.empty()) {
        return;
    }

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NetworkGeometryProcessor requires VKBuffer, got different buffer type");
        return;
    }

    for (auto& [name, binding] : m_bindings) {
        if (!binding.network || !binding.network->is_enabled()) {
            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' disabled, skipping upload", name);
            continue;
        }

        auto* operator_ptr = binding.network->get_operator();
        if (!operator_ptr) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' has no operator", name);
            continue;
        }

        auto* graphics_op = dynamic_cast<Nodes::Network::GraphicsOperator*>(operator_ptr);
        if (!graphics_op) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' operator '{}' is not a GraphicsOperator",
                name, operator_ptr->get_type_name());
            continue;
        }

        auto vertex_data = graphics_op->get_vertex_data();
        size_t vertex_count = graphics_op->get_vertex_count();

        if (vertex_data.empty() || vertex_count == 0) {
            if (binding.gpu_vertex_buffer->is_host_visible()) {
                binding.gpu_vertex_buffer->clear();
            }
            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' has no vertices, cleared buffer", name);
            continue;
        }

        size_t required_size = vertex_data.size();
        size_t available_size = binding.gpu_vertex_buffer->get_size_bytes();

        if (required_size > available_size) {
            auto new_size = static_cast<size_t>(required_size * 1.5F);

            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Network '{}' growing: resizing GPU buffer from {} → {} bytes",
                name, available_size, new_size);

            binding.gpu_vertex_buffer->resize(new_size, false);

            if (binding.staging_buffer) {
                binding.staging_buffer->resize(new_size, false);
                MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Resized staging buffer for '{}' to {} bytes", name, new_size);
            }
        }

        upload_to_gpu(
            vertex_data.data(),
            vertex_data.size(),
            binding.gpu_vertex_buffer,
            binding.staging_buffer);

        auto layout = graphics_op->get_vertex_layout();

        binding.gpu_vertex_buffer->set_vertex_layout(layout);

        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Uploaded {} vertices from network '{}' ({} bytes, {} operator)",
            vertex_count, name, vertex_data.size(), operator_ptr->get_type_name());
    }
}

} // namespace MayaFlux::Buffers
