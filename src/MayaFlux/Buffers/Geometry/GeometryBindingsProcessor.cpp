#include "GeometryBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

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

    m_bindings[name] = GeometryBinding {
        .node = node,
        .gpu_vertex_buffer = vertex_buffer,
        .staging_buffer = staging
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound geometry node '{}' ({} vertices, {} bytes, stride: {})",
        name, node->get_vertex_count(), vertex_data_size, node->get_vertex_stride());
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

void GeometryBindingsProcessor::processing_function(std::shared_ptr<Buffer> buffer)
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

        size_t required_size = vertices.size_bytes();
        size_t available_size = binding.gpu_vertex_buffer->get_size_bytes();

        if (required_size > available_size) {
            auto new_size = static_cast<size_t>(required_size * 1.5F);

            MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Geometry '{}' growing: resizing GPU buffer from {} → {} bytes",
                name, available_size, new_size);

            binding.gpu_vertex_buffer->resize(new_size, false);
            available_size = new_size;

            if (binding.staging_buffer) {
                binding.staging_buffer->resize(new_size, false);
                MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "Resized staging buffer for '{}' to {} bytes", name, new_size);
            }
        }

        size_t upload_size = std::min<size_t>(required_size, available_size);

        upload_to_gpu(
            vertices.data(),
            upload_size,
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
                size_t required_size = vertices.size_bytes();
                size_t available_size = vk_buffer->get_size_bytes();

                if (required_size > available_size) {
                    auto new_size = static_cast<size_t>(required_size * 1.5F);

                    MF_INFO(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                        "Fallback geometry growing: resizing GPU buffer from {} → {} bytes",
                        available_size, new_size);

                    vk_buffer->resize(new_size, false);
                    available_size = new_size;
                }

                size_t upload_size = std::min<size_t>(required_size, available_size);

                std::shared_ptr<VKBuffer> staging = vk_buffer->is_host_visible() ? nullptr : first_binding.staging_buffer;

                upload_to_gpu(
                    vertices.data(),
                    upload_size,
                    vk_buffer,
                    staging);

                if (first_binding.node->get_vertex_layout()) {
                    vk_buffer->set_vertex_layout(
                        first_binding.node->get_vertex_layout().value());
                }
            }
            first_binding.node->clear_gpu_update_flag();
        }
    }
}

} // namespace MayaFlux::Buffers
