#include "FormaBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/BufferUploadProcessor.hpp"
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
    auto upload = std::make_shared<Buffers::BufferUploadProcessor>();
    set_default_processor(upload);

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

    get_processing_chain()->add_final_processor(m_render_processor, shared_from_this());

    set_default_render_config(resolved_config);
}

void FormaBuffer::write_bytes(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty())
        return;

    if (bytes.size() > get_size()) {
        MF_RT_WARN(Journal::Component::Portal, Journal::Context::BufferProcessing,
            "FormaBuffer::write_bytes: {} bytes exceeds capacity {}, skipping",
            bytes.size(), get_size());
        return;
    }

    auto& res = get_buffer_resources();
    if (!res.mapped_ptr) {
        MF_RT_WARN(Journal::Component::Portal, Journal::Context::BufferProcessing,
            "FormaBuffer::write_bytes: buffer not host-visible or not yet registered");
        return;
    }

    std::memcpy(res.mapped_ptr, bytes.data(), bytes.size());
    mark_dirty_range(0, bytes.size());
}

} // namespace MayaFlux::Portal::Forma
