#include "Sinks.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/BufferProcessingChain.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/AudioWriteProcessor.hpp"
#include "MayaFlux/Buffers/Staging/DataWriteProcessor.hpp"
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
    std::function<Kakshya::DataVariant(const InfluenceContext&)> fn,
    std::string fn_name)
{
    auto buf = std::make_shared<Buffers::AudioBuffer>(channel, Buffers::s_preferred_buffer_size);
    auto writer = std::make_shared<Buffers::AudioWriteProcessor>();
    buf->set_default_processor(writer);

    mgr.add_buffer(buf, Buffers::ProcessingToken::AUDIO_BACKEND, channel);

    sinks.push_back(AudioSink {
        .buf = std::move(buf),
        .writer = std::move(writer),
        .fn = std::move(fn),
        .fn_name = std::move(fn_name),
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
    RenderFn fn,
    std::string fn_name,
    const std::optional<glm::vec3>& initial_position)
{
    constexpr size_t k_initial_bytes = 4096;

    auto buf = std::make_shared<Buffers::VKBuffer>(
        k_initial_bytes,
        Buffers::VKBuffer::Usage::VERTEX,
        Kakshya::DataModality::VERTICES_3D);

    auto writer = std::make_shared<Buffers::DataWriteProcessor>();
    buf->set_default_processor(writer);
    mgr.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);

    auto rc = config;

    if (rc.vertex_shader.empty() || rc.fragment_shader.empty()) {
        switch (rc.topology) {
        case Portal::Graphics::PrimitiveTopology::LINE_LIST:
        case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
            if (rc.vertex_shader.empty())
                rc.vertex_shader = "line.vert.spv";
            if (rc.fragment_shader.empty())
                rc.fragment_shader = "line.frag.spv";
#ifndef MAYAFLUX_PLATFORM_MACOS
            if (rc.geometry_shader.empty())
                rc.geometry_shader = "line.geom.spv";
#else
            rc.topology = Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST;
#endif
            break;
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
        case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
            if (rc.vertex_shader.empty())
                rc.vertex_shader = "triangle.vert.spv";
            if (rc.fragment_shader.empty())
                rc.fragment_shader = "triangle.frag.spv";
            break;
        default:
            if (rc.vertex_shader.empty())
                rc.vertex_shader = "point.vert.spv";
            if (rc.fragment_shader.empty())
                rc.fragment_shader = "point.frag.spv";
            break;
        }
    }

    auto renderer = std::make_shared<Buffers::RenderProcessor>(Buffers::ShaderConfig { rc.vertex_shader });
    renderer->set_fragment_shader(rc.fragment_shader);

    if (!rc.geometry_shader.empty())
        renderer->set_geometry_shader(rc.geometry_shader);

    renderer->set_primitive_topology(rc.topology);
    renderer->set_polygon_mode(rc.polygon_mode);
    renderer->set_cull_mode(rc.cull_mode);

    renderer->enable_depth_test();
    buf->set_needs_depth_attachment(true);

    renderer->set_target_window(rc.target_window, buf);
    buf->set_render_processor(renderer);
    buf->get_processing_chain()->add_final_processor(renderer, buf);

    auto& sink = sinks.emplace_back(RenderSink {
        .buf = buf,
        .writer = std::move(writer),
        .renderer = std::move(renderer),
        .fn = std::move(fn),
        .fn_name = std::move(fn_name),
        .window = rc.target_window,
    });

    if (!sink.fn && initial_position.has_value()) {
        sink.writer->set_data(std::vector<Kakshya::DataVariant> {
            Kakshya::DataVariant { std::vector<glm::vec3> { *initial_position } },
        });
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

void dispatch_render_sinks(std::vector<RenderSink>& sinks, const InfluenceContext& ctx)
{
    for (auto& s : sinks) {
        if (s.fn) {
            s.fn(ctx);
        }
    }
}

} // namespace MayaFlux::Nexus
