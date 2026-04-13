#include "Rigs.hpp"

#include "MayaFlux/API/Chronie.hpp"
#include "MayaFlux/API/Config.hpp"
#include "MayaFlux/API/Depot.hpp"
#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/IO/IOManager.hpp"
#include "MayaFlux/Kriya/SamplingPipeline.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux {

template <size_t N>
std::shared_ptr<Kriya::SamplingPipeline<N>> create_sampler(
    const std::string& filepath, uint32_t num_samples, bool truncate,
    uint32_t channel)
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

    auto sampler = std::make_shared<Kriya::SamplingPipeline<N>>(
        std::move(stream), mgr, sched, channel, buf_size);

    sampler->build();
    return sampler;
}

template std::shared_ptr<Kriya::SamplingPipeline<2>>
create_sampler<2>(const std::string&, uint32_t, bool, uint32_t);
template std::shared_ptr<Kriya::SamplingPipeline<4>>
create_sampler<4>(const std::string&, uint32_t, bool, uint32_t);
template std::shared_ptr<Kriya::SamplingPipeline<8>>
create_sampler<8>(const std::string&, uint32_t, bool, uint32_t);

} // namespace MayaFlux
