#include "Sinks.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Geometry/GeometryWriteProcessor.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/EigenAccess.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nexus {

// =============================================================================
// Audio
// =============================================================================

void add_audio_sink(
    std::vector<AudioSink>& sinks,
    Buffers::BufferManager& mgr,
    uint32_t channel,
    std::function<Kakshya::DataVariant(const InfluenceContext&)> fn)
{
    auto buf = std::make_shared<Buffers::AudioBuffer>(channel, Buffers::s_preferred_buffer_size);
    auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
    buf->set_default_processor(writer);

    mgr.add_buffer(buf, Buffers::ProcessingToken::AUDIO_BACKEND, channel);

    sinks.push_back(AudioSink {
        .buf = std::move(buf),
        .writer = std::move(writer),
        .fn = std::move(fn),
        .channel = channel,
    });

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Nexus: audio sink added on channel {}", channel);
}

void remove_audio_sink(
    std::vector<AudioSink>& sinks,
    Buffers::BufferManager& mgr,
    uint32_t channel)
{
    auto it = std::ranges::find_if(sinks,
        [channel](const AudioSink& s) { return s.channel == channel; });

    if (it == sinks.end()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "Nexus: remove_audio_sink called for channel {} but no sink found", channel);
        return;
    }

    mgr.remove_buffer(it->buf, Buffers::ProcessingToken::AUDIO_BACKEND, channel);
    sinks.erase(it);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Nexus: audio sink removed from channel {}", channel);
}

void push_audio_data(std::vector<AudioSink>& sinks, std::span<const double> samples)
{
    for (auto& s : sinks) {
        s.writer->set_data(samples);
    }
}

void dispatch_audio_sinks(std::vector<AudioSink>& sinks, const InfluenceContext& ctx)
{
    for (auto& s : sinks) {
        if (!s.fn) {
            continue;
        }
        auto variant = s.fn(ctx);

        if (auto* vec = std::get_if<std::vector<double>>(&variant)) {
            s.writer->set_data(std::span<const double>(*vec));
        } else {
            Kakshya::EigenAccess acc(variant);
            const Eigen::VectorXd v = acc.to_vector();
            std::vector<double> samples(static_cast<size_t>(v.size()));
            Eigen::Map<Eigen::VectorXd>(samples.data(), v.size()) = v;
            s.writer->set_data(std::span<const double>(samples));
        }
    }
}

// =============================================================================
// Render
// =============================================================================

void add_render_sink(
    std::vector<RenderSink>& sinks,
    Buffers::BufferManager& mgr,
    const Portal::Graphics::RenderConfig& config,
    std::function<Kakshya::DataVariant(const InfluenceContext&)> fn,
    const std::optional<glm::vec3>& initial_position)
{
    constexpr size_t k_initial_bytes = 4096;

    auto buf = std::make_shared<Buffers::VKBuffer>(
        k_initial_bytes,
        Buffers::VKBuffer::Usage::VERTEX,
        Kakshya::DataModality::VERTEX_POSITIONS_3D);

    auto writer = std::make_shared<Buffers::GeometryWriteProcessor>();

    const bool is_line = config.topology == Portal::Graphics::PrimitiveTopology::LINE_LIST
        || config.topology == Portal::Graphics::PrimitiveTopology::LINE_STRIP;
    const bool is_mesh = config.topology == Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST
        || config.topology == Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP;

    if (is_line) {
        writer->set_mode(Buffers::GeometryWriteMode::LINE);
    } else if (is_mesh) {
        writer->set_mode(Buffers::GeometryWriteMode::MESH);
    } else {
        writer->set_mode(Buffers::GeometryWriteMode::POINT);
    }

    buf->set_default_processor(writer);

    mgr.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    std::string vert = config.vertex_shader;
    std::string frag = config.fragment_shader;
    std::string geom = config.geometry_shader;

    if (vert.empty() || frag.empty()) {
        switch (config.topology) {
        case Portal::Graphics::PrimitiveTopology::LINE_LIST:
        case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
#ifndef MAYAFLUX_PLATFORM_MACOS
            if (vert.empty())
                vert = "line.vert.spv";
            if (frag.empty())
                frag = "line.frag.spv";
            if (geom.empty())
                geom = "line.geom.spv";
#else
            if (vert.empty())
                vert = "line_fallback.vert.spv";
            if (frag.empty())
                frag = "line_fallback.frag.spv";
#endif
            break;
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
            if (vert.empty())
                vert = "triangle.vert.spv";
            if (frag.empty())
                frag = "triangle.frag.spv";
            break;
        default:
            if (vert.empty())
                vert = "point.vert.spv";
            if (frag.empty())
                frag = "point.frag.spv";
            break;
        }
    }

    auto renderer = std::make_shared<Buffers::RenderProcessor>(Buffers::ShaderConfig { vert });
    renderer->set_fragment_shader(frag);
    if (!geom.empty())
        renderer->set_geometry_shader(geom);

    renderer->set_primitive_topology(config.topology);
    renderer->set_polygon_mode(config.polygon_mode);
    renderer->set_cull_mode(config.cull_mode);
    renderer->set_target_window(config.target_window, buf);
    buf->get_processing_chain()->add_final_processor(renderer, buf);

    auto& sink = sinks.emplace_back(RenderSink {
        .buf = buf,
        .writer = std::move(writer),
        .renderer = std::move(renderer),
        .fn = std::move(fn),
        .window = config.target_window,
    });

    if (!sink.fn && initial_position.has_value()) {
        sink.writer->set_data(
            Kakshya::DataVariant { std::vector<glm::vec3> { *initial_position } });
    }

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Nexus: render sink added");
}

void remove_render_sink(
    std::vector<RenderSink>& sinks,
    Buffers::BufferManager& mgr,
    const std::shared_ptr<Core::Window>& window)
{
    auto it = std::ranges::find_if(sinks,
        [&window](const RenderSink& s) { return s.window == window; });

    if (it == sinks.end()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "Nexus: remove_render_sink called but no sink found for given window");
        return;
    }

    mgr.remove_buffer(it->buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    sinks.erase(it);

    MF_DEBUG(Journal::Component::Buffers, Journal::Context::Init,
        "Nexus: render sink removed");
}

void push_geometry(std::vector<RenderSink>& sinks, const Kakshya::DataVariant& data)
{
    for (auto& s : sinks) {
        s.writer->set_data(data);
    }
}

void dispatch_render_sinks(std::vector<RenderSink>& sinks, const InfluenceContext& ctx)
{
    for (auto& s : sinks) {
        if (s.fn) {
            s.writer->set_data(s.fn(ctx));
        }
    }
}

} // namespace MayaFlux::Nexus
