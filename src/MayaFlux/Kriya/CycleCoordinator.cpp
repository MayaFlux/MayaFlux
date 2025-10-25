#include "CycleCoordinator.hpp"

#include "BufferPipeline.hpp"

#include "MayaFlux/Kriya/Awaiters.hpp"

namespace MayaFlux::Kriya {

CycleCoordinator::CycleCoordinator(Vruta::TaskScheduler& scheduler)
    : m_scheduler(scheduler)
{
}

Vruta::SoundRoutine CycleCoordinator::sync_pipelines(
    std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
    uint32_t sync_every_n_cycles,
    uint64_t samples_per_cycle)
{

    auto& promise = co_await GetPromise {};
    uint32_t cycle = 0;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        if (cycle % sync_every_n_cycles == 0) {
            for (auto& pipeline_ref : pipelines) {
                auto& pipeline = pipeline_ref.get();
                if (pipeline.has_pending_data()) {
                    std::cout << "Sync point: Pipeline has stale data at cycle " << cycle << '\n';
                }
            }
        }

        for (auto& pipeline_ref : pipelines) {
            pipeline_ref.get().execute_once();
        }

        ++cycle;
        co_await SampleDelay { samples_per_cycle };
    }
}

std::shared_ptr<Vruta::SoundRoutine> CycleCoordinator::sync_pipelines_at_rate(
    std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
    uint32_t sync_every_n_cycles,
    double seconds_per_cycle)
{
    uint64_t samples_per_cycle = m_scheduler.seconds_to_samples(seconds_per_cycle);
    auto routine = sync_pipelines(pipelines, sync_every_n_cycles, samples_per_cycle);
    return std::make_shared<Vruta::SoundRoutine>(std::move(routine));
}

Vruta::SoundRoutine CycleCoordinator::manage_transient_data(
    std::shared_ptr<Buffers::AudioBuffer> buffer,
    std::function<void(uint32_t)> on_data_ready,
    std::function<void(uint32_t)> on_data_expired)
{

    auto& promise = co_await GetPromise {};
    uint32_t cycle = 0;

    while (true) {
        if (promise.should_terminate) {
            break;
        }

        if (buffer->has_data_for_cycle()) {
            on_data_ready(cycle);
            co_await SampleDelay { 1 };

            if (buffer->has_data_for_cycle()) {
                on_data_expired(cycle + 1);
            }
        }

        cycle++;
        co_await SampleDelay { 1 };
    }
}
}
