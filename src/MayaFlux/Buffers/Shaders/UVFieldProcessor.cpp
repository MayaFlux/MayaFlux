#include "UVFieldProcessor.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

#include "MayaFlux/Buffers/VKBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Portal/Graphics/ShaderFoundry.hpp"

namespace MayaFlux::Buffers {

UVFieldProcessor::UVFieldProcessor()
    : ComputeProcessor("uv_field.comp.spv", 64)
{
    m_config.push_constant_size = sizeof(PushConstants);

    m_config.bindings["vertices"] = ShaderBinding(0, 0, vk::DescriptorType::eStorageBuffer);
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
    m_sampler_config = config;
    m_sampler = nullptr;
    m_pc.write_colour = m_texture ? 1U : 0U;
    m_needs_descriptor_rebuild = true;
    m_config.shader_path = m_texture ? "uv_field_colour.comp.spv" : "uv_field.comp.spv";
    if (m_texture) {
        m_config.bindings["tex_sampler"] = ShaderBinding(0, 1, vk::DescriptorType::eCombinedImageSampler);
    } else {
        m_config.bindings.erase("tex_sampler");
    }
    m_initialized = false;
}

void UVFieldProcessor::clear_texture()
{
    m_texture = nullptr;
    m_sampler = nullptr;
    m_pc.write_colour = 0;
    m_needs_descriptor_rebuild = true;
    m_config.shader_path = "uv_field.comp.spv";
    m_config.bindings.erase("tex_sampler");
    m_initialized = false;
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

void UVFieldProcessor::collect_push_writes(
    const std::shared_ptr<VKBuffer>& buffer,
    std::vector<Portal::Graphics::PushDescriptorWrite>& writes)
{
    ComputeProcessor::collect_push_writes(buffer, writes);

    if (!m_texture) {
        return;
    }

    if (!m_sampler) {
        auto& forge = Portal::Graphics::SamplerForge::instance();
        m_sampler = forge.get_or_create(m_sampler_config);
    }

    if (!m_sampler) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "UVFieldProcessor: sampler creation failed, texture will not be sampled");
        return;
    }

    writes.push_back({
        .binding = 1,
        .type = vk::DescriptorType::eCombinedImageSampler,
        .image = vk::DescriptorImageInfo(
            m_sampler, m_texture->get_image_view(), vk::ImageLayout::eShaderReadOnlyOptimal),
    });
}

bool UVFieldProcessor::on_before_execute(
    Portal::Graphics::CommandBufferID /*cmd_id*/,
    const std::shared_ptr<VKBuffer>& buffer)
{
    if (!buffer) {
        return false;
    }

    m_pc.vertex_count = static_cast<uint32_t>(buffer->get_size_bytes() / sizeof(Kakshya::MeshVertex));
    if (m_pc.vertex_count == 0) {
        return false;
    }

    MF_RT_DEBUG(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "UVFieldProcessor dispatch: {} vertices, mode={}, write_colour={}, has_texture={}",
        m_pc.vertex_count, m_pc.mode, m_pc.write_colour, m_texture != nullptr);

    set_push_constant_data(m_pc);
    return true;
}

} // namespace MayaFlux::Buffers
