#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Vruta/Scheduler.hpp"

namespace MayaFlux::Kriya {

class BufferPipeline;

/**
 * @class CycleCoordinator
 * @brief Cross-pipeline synchronization and coordination system.
 *
 * CycleCoordinator provides synchronization mechanisms for coordinating multiple
 * BufferPipeline instances and managing transient data lifecycles. It integrates
 * with the Vruta scheduling system to provide sample-accurate timing coordination
 * across complex processing networks.
 *
 * **Example Usage:**
 * ```cpp
 * CycleCoordinator coordinator(*scheduler);
 *
 * // Synchronize multiple analysis pipelines
 * auto sync_routine = coordinator.sync_pipelines({
 *     std::ref(spectral_pipeline),
 *     std::ref(temporal_pipeline),
 *     std::ref(feature_pipeline)
 * }, 4); // Sync every 4 cycles
 *
 * // Manage transient capture data
 * auto data_routine = coordinator.manage_transient_data(
 *     capture_buffer,
 *     [](uint32_t cycle) { std::cout << "Data ready: " << cycle << std::endl; },
 *     [](uint32_t cycle) { cleanup_expired_data(cycle); }
 * );
 *
 * scheduler->add_task(sync_routine);
 * scheduler->add_task(data_routine);
 * ```
 *
 * @see BufferPipeline For pipeline construction
 * @see BufferCapture For data capture strategies
 * @see Vruta::TaskScheduler For execution scheduling
 */
class CycleCoordinator {
public:
    /**
     * @brief Construct coordinator with task scheduler integration.
     * @param scheduler Vruta task scheduler for timing coordination
     */
    explicit CycleCoordinator(Vruta::TaskScheduler& scheduler);

    /**
     * @brief Create a synchronization routine for multiple pipelines.
     * @param pipelines Vector of pipeline references to synchronize
     * @param sync_every_n_cycles Synchronization interval in cycles
     * @param samples_per_cycle Number of samples per cycle for timing
     * @return SoundRoutine that manages pipeline synchronization
     */
    Vruta::SoundRoutine sync_pipelines(
        std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
        uint32_t sync_every_n_cycles = 1, uint64_t samples_per_cycle = 1);

    /**
     * @brief Create a synchronization routine based on real-time rate.
     * @param pipelines Vector of pipeline references to synchronize
     * @param sync_every_n_cycles Synchronization interval in cycles
     * @param seconds_per_cycle Duration of each cycle in seconds
     * @return SoundRoutine that manages pipeline synchronization
     */
    std::shared_ptr<Vruta::SoundRoutine> sync_pipelines_at_rate(
        std::vector<std::reference_wrapper<BufferPipeline>> pipelines,
        uint32_t sync_every_n_cycles,
        double seconds_per_cycle);

    /**
     * @brief Create a transient data management routine.
     * @param buffer AudioBuffer with transient data lifecycle
     * @param on_data_ready Callback when data becomes available
     * @param on_data_expired Callback when data expires
     * @return SoundRoutine that manages data lifecycle
     */
    Vruta::SoundRoutine manage_transient_data(
        std::shared_ptr<Buffers::AudioBuffer> buffer,
        std::function<void(uint32_t)> on_data_ready,
        std::function<void(uint32_t)> on_data_expired);

private:
    Vruta::TaskScheduler& m_scheduler;
};
}
