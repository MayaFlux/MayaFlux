#include "TextureBindingsProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Nodes/Graphics/TextureNode.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void TextureBindingsProcessor::bind_texture_node(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::TextureNode>& node,
    const std::shared_ptr<VKBuffer>& texture)
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
            "Cannot bind texture node '{}' to null texture buffer", name);
    }

    size_t texture_size = static_cast<size_t>(node->get_width()) * static_cast<size_t>(node->get_height()) * 4 * sizeof(float);

    if (texture->get_size_bytes() < texture_size) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "Texture buffer for '{}' is too small: {} bytes required, {} available",
            name, texture_size, texture->get_size_bytes());
    }

    std::shared_ptr<VKBuffer> staging = nullptr;
    if (!texture->is_host_visible()) {
        staging = create_staging_buffer(texture_size);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Created staging buffer for device-local texture '{}' ({} bytes)",
            name, texture_size);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "No staging needed for host-visible texture '{}'", name);
    }

    m_bindings[name] = TextureBinding {
        .node = node,
        .gpu_texture = texture,
        .staging_buffer = staging
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Bound texture node '{}' ({}x{}, {} bytes)",
        name, node->get_width(), node->get_height(), texture_size);
}

void TextureBindingsProcessor::unbind_texture_node(const std::string& name)
{
    if (m_bindings.erase(name) == 0) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Attempted to unbind non-existent texture node '{}'", name);
    } else {
        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Unbound texture node '{}'", name);
    }
}

std::vector<std::string> TextureBindingsProcessor::get_binding_names() const
{
    std::vector<std::string> names;
    names.reserve(m_bindings.size());
    for (const auto& [name, _] : m_bindings) {
        names.push_back(name);
    }
    return names;
}

void TextureBindingsProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (m_bindings.empty()) {
        return;
    }

    auto vk_buffer = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buffer) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "TextureBindingsProcessor requires VKBuffer, got different buffer type");
        return;
    }

    bool attached_is_host_visible = vk_buffer->is_host_visible();

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

        upload_to_gpu(
            pixels.data(),
            pixels.size_bytes(),
            binding.gpu_texture,
            binding.staging_buffer);

        binding.node->clear_gpu_update_flag();
    }

    if (!m_bindings.empty()) {
        auto& first_binding = m_bindings.begin()->second;

        bool attached_is_target = false;
        for (const auto& [name, binding] : m_bindings) {
            if (binding.gpu_texture == vk_buffer) {
                attached_is_target = true;
                break;
            }
        }

        if (!attached_is_target) {
            auto pixels = first_binding.node->get_pixel_buffer();

            if (!pixels.empty()) {
                std::shared_ptr<VKBuffer> staging = attached_is_host_visible ? nullptr : first_binding.staging_buffer;

                upload_to_gpu(
                    pixels.data(),
                    pixels.size_bytes(),
                    vk_buffer,
                    staging);
            }
        }
    }
}

} // namespace MayaFlux::Buffers
