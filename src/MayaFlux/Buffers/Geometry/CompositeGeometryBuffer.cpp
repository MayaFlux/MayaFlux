#include "CompositeGeometryBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/GeometryWriterNode.hpp"

namespace MayaFlux::Buffers {

CompositeGeometryBuffer::CompositeGeometryBuffer(
    size_t initial_capacity,
    float over_allocate_factor)
    : VKBuffer(
          calculate_initial_size(initial_capacity),
          Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_over_allocate_factor(over_allocate_factor)
{
    MF_INFO(Journal::Component::Buffers, Journal::Context::Init,
        "Created CompositeGeometryBuffer with {} bytes capacity (over-allocate: {:.2f}x)",
        get_size_bytes(), over_allocate_factor);
}

void CompositeGeometryBuffer::remove_geometry(const std::string& name)
{
    if (!m_processor) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "Cannot remove geometry '{}': processor not initialized", name);
        return;
    }

    m_processor->remove_geometry(name);

    std::erase_if(m_render_data,
        [&name](const auto& pair) {
            return pair.first == name;
        });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Removed geometry '{}' from composite buffer", name);
}

std::optional<CompositeGeometryProcessor::GeometryCollection>
CompositeGeometryBuffer::get_collection(const std::string& name) const
{
    if (!m_processor) {
        return std::nullopt;
    }

    return m_processor->get_collection(name);
}

size_t CompositeGeometryBuffer::get_collection_count() const
{
    return m_processor ? m_processor->get_collection_count() : 0;
}

void CompositeGeometryBuffer::setup_processors(ProcessingToken token)
{
    auto self = std::dynamic_pointer_cast<CompositeGeometryBuffer>(shared_from_this());

    m_processor = std::make_shared<CompositeGeometryProcessor>();
    m_processor->set_processing_token(token);

    set_default_processor(m_processor);

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Setup CompositeGeometryProcessor with token {}",
        static_cast<int>(token));
}

void CompositeGeometryBuffer::add_geometry(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
    Portal::Graphics::PrimitiveTopology topology,
    const std::shared_ptr<Core::Window>& target_window)
{
    RenderConfig config;
    config.target_window = target_window;

    switch (topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        config.vertex_shader = "point.vert.spv";
        config.fragment_shader = "point.frag.spv";
        break;

    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
#ifndef MAYAFLUX_PLATFORM_MACOS
        config.vertex_shader = "line.vert.spv";
        config.fragment_shader = "line.frag.spv";
        config.geometry_shader = "line.geom.spv";
#else
        config.vertex_shader = "line_fallback.vert.spv";
        config.fragment_shader = "line_fallback.frag.spv";
#endif
        break;

    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_FAN:
        config.vertex_shader = "triangle.vert.spv";
        config.fragment_shader = "triangle.frag.spv";
        break;

    default:
        config.vertex_shader = "point.vert.spv";
        config.fragment_shader = "point.frag.spv";
    }

    add_geometry(name, node, topology, config);
}

void CompositeGeometryBuffer::add_geometry(
    const std::string& name,
    const std::shared_ptr<Nodes::GpuSync::GeometryWriterNode>& node,
    Portal::Graphics::PrimitiveTopology topology,
    const RenderConfig& config)
{
    if (!node) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Cannot add null geometry node '{}'", name);
    }

    if (!m_processor) {
        error<std::runtime_error>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Must call setup_processors() before add_geometry()");
    }

    if (!config.target_window) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::Init,
            std::source_location::current(),
            "Target window must be specified in RenderConfig");
    }

    m_processor->add_geometry(name, node, topology);
    auto self = std::dynamic_pointer_cast<CompositeGeometryBuffer>(shared_from_this());

    auto render = std::make_shared<RenderProcessor>(
        ShaderConfig { config.vertex_shader });

    render->set_fragment_shader(config.fragment_shader);

    if (!config.geometry_shader.empty()) {
        render->set_geometry_shader(config.geometry_shader);
    }

    if (auto layout = node->get_vertex_layout()) {
        render->set_buffer_vertex_layout(self, *layout);
    }

    render->set_target_window(config.target_window, self);
    render->set_primitive_topology(topology);
    render->set_polygon_mode(config.polygon_mode);
    render->set_cull_mode(config.cull_mode);

    render->set_vertex_range(0, 0);

    auto chain = get_processing_chain();
    chain->add_processor(render, shared_from_this());

    m_render_data[name] = RenderData {
        .render_processor = render,
        .vertex_offset = 0,
        .vertex_count = 0
    };

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Added geometry '{}' to composite buffer (topology: {}, shaders: {}/{})",
        name, static_cast<int>(topology), config.vertex_shader, config.fragment_shader);
}

void CompositeGeometryBuffer::setup_rendering(const RenderConfig& /*config*/)
{
    MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
        "setup_rendering() is deprecated for CompositeGeometryBuffer. "
        "Use add_geometry() with RenderConfig instead.");
}

size_t CompositeGeometryBuffer::calculate_initial_size(size_t requested_capacity)
{
    constexpr size_t MIN_SIZE = 1024;
    return std::max(requested_capacity, MIN_SIZE);
}

void CompositeGeometryBuffer::update_collection_render_range(
    const std::string& name,
    uint32_t vertex_offset,
    uint32_t vertex_count)
{
    auto it = m_render_data.find(name);
    if (it != m_render_data.end()) {
        it->second.vertex_offset = vertex_offset;
        it->second.vertex_count = vertex_count;

        it->second.render_processor->set_vertex_range(vertex_offset, vertex_count);

        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "Updated render range for '{}': offset={}, count={}",
            name, vertex_offset, vertex_count);
    }
}

void CompositeGeometryBuffer::update_collection_vertex_layout(
    const std::string& name,
    const Kakshya::VertexLayout& layout)
{
    auto it = m_render_data.find(name);
    if (it == m_render_data.end()) {
        return;
    }

    it->second.render_processor->set_buffer_vertex_layout(
        std::dynamic_pointer_cast<VKBuffer>(shared_from_this()),
        layout);

    MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "Updated vertex layout for '{}': stride={}, vertices={}",
        name, layout.stride_bytes, layout.vertex_count);
}

} // namespace MayaFlux::Buffers
