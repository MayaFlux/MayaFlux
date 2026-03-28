#include "GeometryBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

namespace MayaFlux::Buffers {

GeometryBindingsProcessor::GeometryBindingsProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void GeometryBindingsProcessor::bind_geometry_node(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
    const std::shared_ptr<VKBuffer>& vertex_buffer)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind null geometry node '{}'", name);
    }

    if (!vertex_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind geometry node '{}' to null vertex buffer", name);
    }

    size_t vertex_data_size = node->get_vertex_buffer_size_bytes();

    if (vertex_buffer->get_size_bytes() < vertex_data_size) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Vertex buffer for '{}' may be too small: {} bytes required, {} available. "
            "Will upload partial data.",
            name, vertex_data_size, vertex_buffer->get_size_bytes());
    }

    std::shared_ptr<VKBuffer> staging = nullptr;
    if (!vertex_buffer->is_host_visible()) {
        size_t staging_size = std::max<size_t>(vertex_buffer->get_size_bytes(), vertex_data_size);
        staging = create_staging_buffer(staging_size);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created staging buffer for device-local geometry '{}' ({} bytes)",
            name, staging_size);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No staging needed for host-visible geometry '{}'", name);
    }

    std::shared_ptr<VKBuffer> index_buf = nullptr;
    std::shared_ptr<VKBuffer> index_staging_buf = nullptr;

    if (node->has_indices()) {
        const size_t index_data_size = node->get_index_count() * sizeof(uint32_t);
        index_buf = std::make_shared<VKBuffer>(
            index_data_size,
            VKBuffer::Usage::INDEX,
            Kakshya::DataModality::UNKNOWN);

        ensure_initialized(index_buf);

        index_staging_buf = create_staging_buffer(index_data_size);
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created index staging buffer for device-local geometry '{}' ({} bytes)",
            name, index_data_size);
    }

    m_bindings[name] = GeometryBinding {
        .node = node,
        .gpu_vertex_buffer = vertex_buffer,
        .staging_buffer = staging,
        .gpu_index_buffer = index_buf,
        .index_staging_buffer = index_staging_buf
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound geometry node '{}' ({} vertices, {} bytes, stride: {}, indices: {})",
        name, node->get_vertex_count(), vertex_data_size, node->get_vertex_stride(),
        node->get_index_count());
}

void GeometryBindingsProcessor::unbind_geometry_node(const std::string& name)
{
    if (m_bindings.erase(name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to unbind non-existent geometry node '{}'", name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound geometry node '{}'", name);
    }
}

bool GeometryBindingsProcessor::has_binding(const std::string& name) const
{
    return m_bindings.contains(name);
}

std::vector<std::string> GeometryBindingsProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

size_t GeometryBindingsProcessor::get_binding_count() const
{
    return m_bindings.size();
}

std::optional<GeometryBindingsProcessor::GeometryBinding>
GeometryBindingsProcessor::get_binding(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second;
    }
    return std::nullopt;
}

void GeometryBindingsProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_bindings.empty()) {
        return;
    }

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "GeometryBindingsProcessor requires VKBuffer, got different buffer type");
        return;
    }

    for (auto& [name, binding] : m_bindings) {
        if (!binding.node->needs_gpu_update()) {
            MF_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Geometry '{}' unchanged, skipping upload", name);
            continue;
        }

        auto vertices = binding.node->get_vertex_data();

        if (vertices.empty()) {
            if (binding.gpu_vertex_buffer->is_host_visible()) {
                binding.gpu_vertex_buffer->clear();
            }

            if (binding.node->get_vertex_layout()) {
                binding.gpu_vertex_buffer->set_vertex_layout(
                    binding.node->get_vertex_layout().value());
            }

            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Geometry '{}' cleared", name);
            continue;
        }

        upload_resizing(
            vertices.data(),
            vertices.size_bytes(),
            binding.gpu_vertex_buffer,
            binding.staging_buffer);

        if (binding.node->get_vertex_layout()) {
            binding.gpu_vertex_buffer->set_vertex_layout(
                binding.node->get_vertex_layout().value());

            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Set vertex layout for '{}' ({} vertices, {} attributes)",
                name,
                binding.node->get_vertex_count(),
                binding.node->get_vertex_layout()->attributes.size());
        } else {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Geometry node '{}' has no vertex layout. "
                "RenderProcessor may fail without layout info.",
                name);
        }

        upload_index_data(name, binding);
        binding.node->clear_gpu_update_flag();
    }

    bool attached_is_target = false;
    for (const auto& [name, binding] : m_bindings) {
        if (binding.gpu_vertex_buffer == vk_buffer) {
            attached_is_target = true;
            break;
        }
    }

    if (!attached_is_target && !m_bindings.empty()) {
        auto& first_binding = m_bindings.begin()->second;
        if (first_binding.node->needs_gpu_update()) {
            auto vertices = first_binding.node->get_vertex_data();

            if (!vertices.empty()) {
                upload_resizing(
                    vertices.data(),
                    vertices.size_bytes(),
                    vk_buffer,
                    first_binding.staging_buffer);

                if (first_binding.node->get_vertex_layout()) {
                    vk_buffer->set_vertex_layout(
                        first_binding.node->get_vertex_layout().value());
                }
            }
            upload_index_data(m_bindings.begin()->first, first_binding);
            first_binding.node->clear_gpu_update_flag();
        }
    }
}

void GeometryBindingsProcessor::upload_index_data(
    const std::string& name,
    GeometryBinding& binding)
{
    if (!binding.node->has_indices()) {
        return;
    }

    const auto indices = binding.node->get_index_data();
    const size_t required = indices.size_bytes();

    if (required == 0) {
        return;
    }

    if (!binding.gpu_index_buffer) {
        binding.gpu_index_buffer = std::make_shared<VKBuffer>(
            required,
            VKBuffer::Usage::INDEX,
            Kakshya::DataModality::UNKNOWN);

        ensure_initialized(binding.gpu_index_buffer);

        binding.index_staging_buffer = create_staging_buffer(required);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Lazily created index buffer for '{}' ({} bytes)", name, required);
    }

    upload_resizing(
        indices.data(),
        required,
        binding.gpu_index_buffer,
        binding.index_staging_buffer);

    binding.gpu_vertex_buffer->set_index_resources(
        binding.gpu_index_buffer->get_buffer(),
        binding.gpu_index_buffer->get_buffer_resources().memory,
        binding.gpu_index_buffer->get_size_bytes());

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Uploaded {} indices ({} bytes) for '{}'",
        binding.node->get_index_count(), required, name);
}

} // namespace MayaFlux::Buffers
