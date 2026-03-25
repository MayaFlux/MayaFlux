#include "UVFieldProcessor.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Buffers {

UVFieldProcessor::UVFieldProcessor()
    : ComputeProcessor("uv_field.comp.spv", 64)
{
    m_config.push_constant_size = sizeof(PushConstants);

    m_config.bindings["vertices"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);

    m_config.bindings["tex_sampler"] = ShaderBinding(0, 1, vk::DescriptorType::eCombinedImageSampler);
}

// =============================================================================
// Projection configuration
// =============================================================================

void UVFieldProcessor::set_projection(UVProjectionMode mode)
{
    m_pc.mode = static_cast<uint32_t>(mode);
}

void UVFieldProcessor::set_origin(const glm::vec3& origin)
{
    m_pc.param_origin = origin;
}

void UVFieldProcessor::set_axis(const glm::vec3& axis)
{
    m_pc.param_axis = axis;
}

void UVFieldProcessor::set_scale(float scale)
{
    m_pc.param_scale = scale;
}

void UVFieldProcessor::set_aux(float aux)
{
    m_pc.param_aux = aux;
}

// =============================================================================
// Texture source
// =============================================================================

void UVFieldProcessor::set_texture(
    std::shared_ptr<Core::VKImage> image,
    const Portal::Graphics::SamplerConfig& config)
{
    m_texture = std::move(image);

    if (m_texture) {
        m_sampler = Portal::Graphics::get_sampler_factory().get_or_create(config);
        m_pc.write_colour = 1;
    } else {
        m_sampler = nullptr;
        m_pc.write_colour = 0;
    }

    m_needs_descriptor_rebuild = true;
}

void UVFieldProcessor::clear_texture()
{
    m_texture = nullptr;
    m_sampler = nullptr;
    m_pc.write_colour = 0;
    m_needs_descriptor_rebuild = true;
}

// =============================================================================
// ShaderProcessor hooks
// =============================================================================

void UVFieldProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    ComputeProcessor::on_attach(buffer);

    auto vk_buf = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk_buf) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "UVFieldProcessor requires a VKBuffer");
        return;
    }

    bind_buffer("vertices", vk_buf);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "UVFieldProcessor attached ({} bytes)", vk_buf->get_size_bytes());
}

void UVFieldProcessor::on_descriptors_created()
{
    if (m_descriptor_set_ids.empty()) {
        return;
    }

    auto& foundry = Portal::Graphics::get_shader_foundry();

    if (m_texture && m_sampler) {
        foundry.update_descriptor_image(
            m_descriptor_set_ids[0],
            1,
            m_texture->get_image_view(),
            m_sampler,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "UVFieldProcessor: texture descriptor written (binding 1)");
    } else {
        auto dummy = Portal::Graphics::get_texture_manager().get_default_sampler();
        if (dummy) {
            MF_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "UVFieldProcessor: no texture bound, colour write disabled");
        }
    }
}

bool UVFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer) {
        return false;
    }

    m_pc.vertex_count = static_cast<uint32_t>(buffer->get_size_bytes() / 60U);
    if (m_pc.vertex_count == 0) {
        return false;
    }

    set_push_constant_data(m_pc);
    return true;
}

} // namespace MayaFlux::Buffers
