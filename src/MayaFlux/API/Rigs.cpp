#include "Rigs.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kriya/SamplingPipeline.hpp"

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

} // namespace MayaFlux
