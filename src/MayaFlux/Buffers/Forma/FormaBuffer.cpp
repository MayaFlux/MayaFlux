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
    RenderConfig resolved_config = config;
    resolved_config.topology = m_topology;

    switch (resolved_config.topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "point.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "point.frag.spv";
        break;

    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "line.frag.spv";

#ifndef MAYAFLUX_PLATFORM_MACOS
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "line.vert.spv";
        if (config.geometry_shader.empty())
            resolved_config.geometry_shader = "line.geom.spv";
#else
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "line_fallback.vert.spv";

        resolved_config.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif

        break;

    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "triangle.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "triangle.frag.spv";
        break;

    default:
        if (config.vertex_shader.empty())
            resolved_config.vertex_shader = "point.vert.spv";
        if (config.fragment_shader.empty())
            resolved_config.fragment_shader = "point.frag.spv";
    }

    if (!m_render_processor) {
        m_render_processor = std::make_shared<RenderProcessor>(ShaderConfig { resolved_config.vertex_shader });
    } else {
        m_render_processor->set_shader(resolved_config.vertex_shader);
    }

    m_render_processor->set_fragment_shader(resolved_config.fragment_shader);
    if (!resolved_config.geometry_shader.empty()) {
        m_render_processor->set_geometry_shader(resolved_config.geometry_shader);
    }
    m_render_processor->set_target_window(config.target_window, std::dynamic_pointer_cast<VKBuffer>(shared_from_this()));
    m_render_processor->set_primitive_topology(resolved_config.topology);
    m_render_processor->set_polygon_mode(config.polygon_mode);
    m_render_processor->set_cull_mode(config.cull_mode);
    m_render_processor->enable_alpha_blending();

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
}

void FormaBuffer::write_bytes(const std::vector<uint8_t>& bytes)
{
    if (!m_processor) {
        MF_RT_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::write_bytes called before setup_processors");
        return;
    }
    m_processor->set_bytes(bytes);
}

} // namespace MayaFlux::Portal::Forma
