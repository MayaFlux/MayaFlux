#include "NodeTextureProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Buffers/Textures/NodeTextureBuffer.hpp"
#include "MayaFlux/Nodes/Graphics/TextureNode.hpp"
#include "MayaFlux/Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/Registry/BackendRegistry.hpp"
#include "MayaFlux/Registry/Service/BufferService.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NodeTextureProcessor::NodeTextureProcessor()
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
    initialize_buffer_service();
}

void NodeTextureProcessor::bind_texture_node(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::TextureNode>& node,
    const std::shared_ptr<Core::VKImage>& texture)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind null texture node '{}'", name);
    }

    if (!texture) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Cannot bind texture node '{}' to null VKImage", name);
    }

    size_t texture_size = static_cast<size_t>(node->get_width())
        * static_cast<size_t>(node->get_height())
        * 4
        * sizeof(float);

    m_bindings[name] = TextureBinding {
        .node = node,
        .gpu_texture = texture
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound texture node '{}' ({}x{}, {} bytes)",
        name, node->get_width(), node->get_height(), texture_size);
}

void NodeTextureProcessor::unbind_texture_node(const std::string& name)
{
    if (m_bindings.erase(name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to unbind non-existent texture node '{}'", name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound texture node '{}'", name);
    }
}

std::vector<std::string> NodeTextureProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

std::optional<NodeTextureProcessor::TextureBinding>
NodeTextureProcessor::get_binding(const std::string& name) const
{
    auto it = m_bindings.find(name);
    if (it != m_bindings.end()) {
        return it->second;
    }
    return std::nullopt;
}

void NodeTextureProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_bindings.empty()) {
        return;
    }

    auto staging_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!staging_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NodeTextureProcessor requires VKBuffer for staging");
        return;
    }

    auto& loom = Portal::Graphics::get_texture_manager();

    for (auto& [name, binding] : m_bindings) {
        if (!binding.node->needs_gpu_update()) {
            MF_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Texture '{}' unchanged, skipping upload", name);
            continue;
        }

        auto pixels = binding.node->get_pixel_buffer();

        if (pixels.empty()) {
            MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "Texture node '{}' has empty pixel buffer, skipping upload", name);
            continue;
        }

        loom.upload_data(
            binding.gpu_texture,
            pixels.data(),
            pixels.size_bytes());

        binding.node->clear_gpu_update_flag();

        MF_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Uploaded texture '{}' ({} bytes)", name, pixels.size_bytes());
    }
}

void NodeTextureProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    m_attached_buffer = std::dynamic_pointer_cast<NodeTextureBuffer>(buffer);
    if (!m_attached_buffer) {
        return;
    }

    if (!m_buffer_service) {
        m_buffer_service = Registry::BackendRegistry::instance()
                               .get_service<Registry::Service::BufferService>();
    }

    if (!m_buffer_service) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "TextureProcessor requires a valid buffer service");
    }

    if (!m_attached_buffer->is_initialized()) {
        try {
            m_buffer_service->initialize_buffer(m_attached_buffer);
        } catch (const std::exception& e) {
            error_rethrow(
                Journal::Component::Buffers,
                Journal::Context::BufferProcessing,
                std::source_location::current(),
                "Failed to initialize texture buffer: {}", e.what());
        }
    }

    initialize_gpu_resources();
}

void NodeTextureProcessor::initialize_gpu_resources()
{
    if (!m_attached_buffer || m_attached_buffer->m_vertex_bytes.empty()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "NodeTextureProcessor: no attached buffer or empty vertex data");
        return;
    }

    upload_to_gpu(
        m_attached_buffer->m_vertex_bytes.data(),
        m_attached_buffer->m_vertex_bytes.size(),
        m_attached_buffer);

    auto& loom = Portal::Graphics::get_texture_manager();

    for (auto& [name, binding] : m_bindings) {
        auto pixels = binding.node->get_pixel_buffer();
        if (!pixels.empty()) {
            loom.upload_data(binding.gpu_texture, pixels.data(), pixels.size_bytes());
        }
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "NodeTextureProcessor: uploaded {} bytes of vertex geometry",
        m_attached_buffer->m_vertex_bytes.size());
}

void NodeTextureProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_attached_buffer.reset();
}

} // namespace MayaFlux::Buffers
