#include "Rigs.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kriya/Chimera.hpp"
#include "MayaFlux/Kriya/SamplingPipeline.hpp"
#include "MayaFlux/Kriya/TapSet.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

std::shared_ptr<Kriya::SamplingPipeline> create_sampler(
    const std::string& filepath, uint32_t num_samples, bool truncate,
    uint32_t channel, uint64_t max_dur_ms)
{
    auto stream = get_io_manager()->load_audio_bounded(filepath, num_samples, truncate);

    if (!stream) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "create_sampler: failed to load '{}'", filepath);
        return nullptr;
    }

    auto& mgr = *get_buffer_manager();
    auto& sched = *get_scheduler();
    const uint32_t buf_size = Config::get_buffer_size();

    auto sampler = std::make_shared<Kriya::SamplingPipeline>(
        std::move(stream), mgr, sched, channel, buf_size);

    if (max_dur_ms > 0) {
        sampler->build_for(max_dur_ms);
    } else {
        sampler->build();
    }

    return sampler;
}

std::shared_ptr<Kriya::SamplingPipeline> create_sampler_from_stream(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream,
    uint32_t channel, uint64_t max_dur_ms)
{
    if (!stream)
        return nullptr;

    auto& mgr = *get_buffer_manager();
    auto& sched = *get_scheduler();
    const uint32_t buf_size = Config::get_buffer_size();

    auto sampler = std::make_shared<Kriya::SamplingPipeline>(
        std::move(stream), mgr, sched, channel, buf_size);

    if (max_dur_ms > 0) {
        sampler->build_for(max_dur_ms);
    } else {
        sampler->build();
    }

    return sampler;
}

std::vector<std::shared_ptr<Kriya::SamplingPipeline>> create_samplers(
    const std::string& filepath, uint32_t num_samples, bool truncate,
    uint64_t max_dur_ms, uint32_t max_channels)
{
    auto stream = get_io_manager()->load_audio_bounded(filepath, num_samples, truncate);

    if (!stream) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "create_samplers: failed to load '{}'", filepath);
        return {};
    }

    const uint32_t ch_count = (max_channels == 0)
        ? stream->get_num_channels()
        : std::min(max_channels, stream->get_num_channels());

    std::vector<std::shared_ptr<Kriya::SamplingPipeline>> result;
    result.reserve(ch_count);

    for (uint32_t i = 0; i < ch_count; ++i)
        result.push_back(create_sampler_from_stream(stream, i, max_dur_ms));

    return result;
}

Kriya::TapSetBuilder create_tap_set(
    const std::string& filepath, uint32_t num_samples, bool truncate)
{
    auto stream = get_io_manager()->load_audio_bounded(filepath, num_samples, truncate);

    if (!stream) {
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO,
            "create_tap_set: failed to load '{}'", filepath);
    }

    return create_tap_set_from_stream(std::move(stream));
}

Kriya::TapSetBuilder create_tap_set_from_stream(
    std::shared_ptr<Kakshya::DynamicSoundStream> stream)
{
    return Kriya::TapSetBuilder(
        std::move(stream), *get_buffer_manager(), *get_scheduler(), Config::get_buffer_size());
}

Kriya::ChimeraBuilder create_chimera(std::shared_ptr<Buffers::TextureArrayBuffer> buffer)
{
    return Kriya::ChimeraBuilder(std::move(buffer), *get_scheduler());
}

std::shared_ptr<Kakshya::DynamicSoundStream> create_ring(double seconds, uint32_t channels)
{
    const uint32_t rate = Config::get_sample_rate();
    const auto capacity = static_cast<uint64_t>(seconds * static_cast<double>(rate));

    if (capacity == 0 || channels == 0) {
        MF_ERROR(Journal::Component::API, Journal::Context::Configuration,
            "create_ring: {} seconds on {} channels is empty", seconds, channels);
        return nullptr;
    }

    auto ring = std::make_shared<Kakshya::DynamicSoundStream>(rate, channels);
    ring->enable_circular_buffer(capacity);
    return ring;
}

std::shared_ptr<Kriya::BufferPipeline> record_into(
    const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
    Kriya::CaptureBuilder source, uint32_t channel)
{
    if (!stream) {
        return nullptr;
    }

    source.for_cycles(1);

    auto pipeline = create_buffer_pipeline();
    *pipeline >> static_cast<Kriya::BufferOperation>(source)
        >> Kriya::BufferOperation::route_to_container(stream, channel);
    pipeline->execute_buffer_rate();

    return pipeline;
}

std::shared_ptr<Kriya::BufferPipeline> record_into(
    const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
    const std::shared_ptr<Buffers::AudioBuffer>& buffer, uint32_t channel)
{
    if (!buffer) {
        return nullptr;
    }

    return record_into(stream, Kriya::BufferOperation::capture_from(buffer), channel);
}

std::vector<std::shared_ptr<Kriya::BufferPipeline>> record_into(
    const std::shared_ptr<Kakshya::DynamicSoundStream>& stream,
    std::vector<Kriya::CaptureBuilder> sources)
{
    std::vector<std::shared_ptr<Kriya::BufferPipeline>> pipelines;

    if (!stream) {
        return pipelines;
    }

    pipelines.reserve(sources.size());
    for (size_t channel = 0; channel < sources.size(); ++channel) {
        pipelines.push_back(record_into(stream, std::move(sources[channel]), static_cast<uint32_t>(channel)));
    }

    return pipelines;
}

} // namespace MayaFlux
