#include "FormaBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Forma/FormaProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

FormaBuffer::FormaBuffer(
    size_t capacity_bytes,
    Portal::Graphics::PrimitiveTopology topology)
    : VKBuffer(capacity_bytes, Buffers::VKBuffer::Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_topology(topology)
{
}

void FormaBuffer::setup_processors(ProcessingToken token)
{
    auto proc = std::make_shared<Buffers::FormaProcessor>(m_topology);
    set_default_processor(proc);
    m_processor = proc;

    auto chain = std::make_shared<Buffers::BufferProcessingChain>();
    chain->set_preferred_token(token);
    set_processing_chain(chain);
}

void FormaBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;
    resolved.topology = m_topology;

    const bool single_tex = !config.default_texture_binding.empty();
    const auto multi_count = static_cast<uint32_t>(config.additional_textures.size());
    const bool multi_tex = multi_count > 0;

    switch (resolved.topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "point.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "point.frag.spv";
        break;

    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "line.frag.spv";
#ifndef MAYAFLUX_PLATFORM_MACOS
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "line.vert.spv";
        if (resolved.geometry_shader.empty())
            resolved.geometry_shader = "line.geom.spv";
#else
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "line_fallback.vert.spv";
        resolved.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif

        break;

    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = multi_tex ? "triangle_multi.vert.spv" : "triangle.vert.spv";
        if (resolved.fragment_shader.empty()) {
            if (multi_tex) {
                resolved.fragment_shader = "forma_multi.frag.spv";
            } else if (single_tex) {
                resolved.fragment_shader = "forma_textured.frag.spv";
            } else {
                resolved.fragment_shader = "triangle.frag.spv";
            }
        }
        break;

    default:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "point.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "point.frag.spv";
        break;
    }

    ShaderConfig sc { resolved.vertex_shader };
    if (multi_tex) {
        const uint32_t total = single_tex ? 1 + multi_count : multi_count;
        sc.bindings["textures"] = ShaderBinding(
            0, 1, vk::DescriptorType::eCombinedImageSampler, total);
    } else if (single_tex) {
        sc.bindings[config.default_texture_binding] = ShaderBinding(
            0, 1, vk::DescriptorType::eCombinedImageSampler);
    }

    if (!m_render_processor) {
        m_render_processor = std::make_shared<RenderProcessor>(sc);
    } else {
        m_render_processor->set_shader(resolved.vertex_shader);
    }

    m_render_processor->set_fragment_shader(resolved.fragment_shader);
    if (!resolved.geometry_shader.empty())
        m_render_processor->set_geometry_shader(resolved.geometry_shader);

    m_render_processor->set_target_window(
        config.target_window,
        std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(resolved.topology);
    m_render_processor->set_polygon_mode(config.polygon_mode);
    m_render_processor->set_cull_mode(config.cull_mode);
    m_render_processor->enable_alpha_blending();

    if (multi_tex) {
        uint32_t slot = 1;
        if (single_tex && !config.default_texture_binding.empty()) {
            auto it = std::ranges::find_if(config.additional_textures,
                [&](const auto& p) { return p.first == config.default_texture_binding; });
            if (it != config.additional_textures.end()) {
                m_render_processor->bind_texture(slot, it->second);
                ++slot;
            }
        }
        for (const auto& [name, img] : config.additional_textures) {
            if (single_tex && name == config.default_texture_binding)
                continue;
            m_render_processor->bind_texture(slot, img);
            ++slot;
        }
    }

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());
    set_default_render_config(resolved);
}

void FormaBuffer::submit(const std::vector<uint8_t>& bytes)
{
    if (!m_processor) {
        MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::submit called before setup_processors");
        return;
    }
    m_processor->set_bytes(bytes);
}

void FormaBuffer::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    if (!m_processor) {
        MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::set_texture called before setup_processors");
        return;
    }
    m_processor->set_texture(std::move(image), std::move(binding));
}

void FormaBuffer::bind_texture(uint32_t array_index, const std::shared_ptr<Core::VKImage>& image)
{
    if (m_render_processor)
        m_render_processor->bind_texture(1 + array_index, image);
}

} // namespace MayaFlux::Buffers
