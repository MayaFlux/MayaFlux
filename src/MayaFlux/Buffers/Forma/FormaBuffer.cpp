#include "FormaBuffer.hpp"

#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Forma/FormaProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Buffers {

namespace {
    constexpr size_t k_capacity_bytes = 4 * 1024 * 1024; // 4 MiB default capacity, can be overridden by constructor
}

FormaBuffer::FormaBuffer(
    size_t capacity_bytes,
    Portal::Graphics::PrimitiveTopology topology)
    : VKBuffer(capacity_bytes, Buffers::VKBuffer::Usage::VERTEX,
          Kakshya::DataModality::VERTEX_POSITIONS_3D)
    , m_topology(topology)
{
}

FormaBuffer::FormaBuffer(
    Portal::Graphics::PrimitiveTopology topology)
    : FormaBuffer(k_capacity_bytes, topology)
{
}

void FormaBuffer::setup_processors(ProcessingToken token)
{
    auto proc = std::make_shared<Buffers::FormaProcessor>(m_topology);
    set_default_processor(proc);
    m_processor = proc;

    auto chain = get_processing_chain();
    if (!chain) {
        chain = std::make_shared<Buffers::BufferProcessingChain>();
        set_processing_chain(chain);
    }
    chain->set_preferred_token(token);

    if (!m_pending_geometry.empty()) {
        m_processor->set_bytes(m_pending_geometry);
        m_pending_geometry.clear();
    }

    for (auto& [img, binding] : m_pending_textures)
        m_processor->set_texture(std::move(img), std::move(binding));

    m_pending_textures.clear();
}

void FormaBuffer::setup_rendering(const RenderConfig& config)
{
    RenderConfig resolved = config;
    resolved.topology = m_topology;
    resolved.triangulate |= Portal::Graphics::needs_triangulation(
        resolved.topology, resolved.polygon_mode);

    const bool single_tex = !config.default_texture_binding.empty();
    const auto multi_count = static_cast<uint32_t>(config.additional_textures.size());
    const bool multi_tex = multi_count > 0;

    switch (resolved.topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "point.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = multi_tex ? "point_multi.frag.spv"
                : (single_tex ? "point_textured.frag.spv" : "point.frag.spv");
        break;

    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        if (resolved.vertex_shader.empty())
            resolved.vertex_shader = "line.vert.spv";
        if (resolved.fragment_shader.empty())
            resolved.fragment_shader = "line.frag.spv";
        if (resolved.triangulate) {
            resolved.geometry_shader.clear();
        } else if (resolved.geometry_shader.empty()) {
            resolved.geometry_shader = "line.geom.spv";
        }

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

    apply_render_config(resolved, sc);
    if (resolved.triangulate
        && resolved.topology == Portal::Graphics::PrimitiveTopology::POINT_LIST) {
        auto mill_spec = m_render_processor->get_mill_spec();
        mill_spec.use_vertex_extent = false;
        m_render_processor->set_mill_spec(mill_spec);
    }
    m_render_processor->set_mill_on_change(true);
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

            if (img)
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
        m_pending_geometry = bytes;
        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::submit called before setup_processors, storing {} bytes for later submission", bytes.size());
        return;
    }
    m_processor->set_bytes(bytes);
}

FormaBuffer::SliceId FormaBuffer::add_slice(
    const std::vector<uint8_t>& bytes,
    std::optional<Portal::Graphics::PrimitiveTopology> topology)
{
    if (!m_processor) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::add_slice called before setup_processors");
        return 0;
    }
    return m_processor->add_slice(bytes, topology);
}

void FormaBuffer::update_slice(SliceId id, const std::vector<uint8_t>& bytes)
{
    if (m_processor)
        m_processor->update_slice(id, bytes);
}

size_t FormaBuffer::slice_count() const noexcept
{
    return m_processor ? m_processor->slice_count() : 0;
}

bool FormaBuffer::has_slice(SliceId id) const noexcept
{
    return m_processor && m_processor->has_slice(id);
}

void FormaBuffer::remove_slice(SliceId id)
{
    if (m_processor)
        m_processor->remove_slice(id);
}

void FormaBuffer::set_slice_visible(SliceId id, bool visible)
{
    if (m_processor)
        m_processor->set_slice_visible(id, visible);
}

void FormaBuffer::bring_slice_to_front(SliceId id)
{
    if (m_processor)
        m_processor->bring_slice_to_front(id);
}

void FormaBuffer::send_slice_to_back(SliceId id)
{
    if (m_processor)
        m_processor->send_slice_to_back(id);
}

void FormaBuffer::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    if (!m_processor) {
        m_pending_textures.emplace_back(std::move(image), std::move(binding));
        MF_RT_TRACE(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "FormaBuffer::set_texture called before setup_processors, storing texture for later submission");
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
