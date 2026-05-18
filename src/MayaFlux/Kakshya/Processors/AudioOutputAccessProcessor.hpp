#pragma once

#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Registry::Service {
struct AudioBackendService;
}

namespace MayaFlux::Kakshya {

/**
 * @class AudioOutputAccessProcessor
 * @brief Default DataProcessor for AudioOutputContainer.
 *
 * Each process() call performs two writes:
 *
 *   1. m_processed_data — deinterleaved snapshot for the current cycle only.
 *      PLANAR:      processed_data[ch] = vector<double>(buffer_size)
 *      INTERLEAVED: processed_data[0]  = vector<double>(buffer_size * channels)
 *
 *   2. m_data (history) — the same deinterleaved per-channel data is appended
 *      to the container's growing corpus via write_frames(). The write head
 *      (get_num_frames()) advances by buffer_size each cycle, enabling
 *      CursorAccessProcessor readers to chase it independently.
 *
 * The snapshot pointer from AudioBackendService::get_output_snapshot is
 * engine-owned and valid only for the duration of process(). Both writes
 * complete before process() returns.
 *
 * on_attach validates that the container is an AudioOutputContainer and
 * caches channel count, buffer size, and organization strategy.
 */
class MAYAFLUX_API AudioOutputAccessProcessor : public DataProcessor {
public:
    /**
     * @brief Construct with the fixed output block size.
     * @param buffer_size Frames per cycle, must match GlobalStreamInfo::buffer_size.
     */
    explicit AudioOutputAccessProcessor(uint32_t buffer_size);

    ~AudioOutputAccessProcessor() override = default;

    /**
     * @brief Validate container type, cache structure, mark ready for processing.
     * @throws std::invalid_argument if container is not an AudioOutputContainer.
     */
    void on_attach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Clear cached state.
     */
    void on_detach(const std::shared_ptr<SignalSourceContainer>& container) override;

    /**
     * @brief Pull engine snapshot, write m_processed_data, append to m_data.
     *
     * No-op if the backend service is unavailable or the snapshot is empty.
     * Logs a trace-level message in those cases rather than erroring, since
     * the first few calls during engine startup may arrive before any cycle
     * has completed.
     */
    void process(const std::shared_ptr<SignalSourceContainer>& container) override;

    [[nodiscard]] bool is_processing() const override { return m_is_processing.load(); }

private:
    uint32_t m_buffer_size { 0 };
    uint32_t m_channel_count { 0 };
    OrganizationStrategy m_organization { OrganizationStrategy::PLANAR };
    std::atomic<bool> m_is_processing { false };

    Registry::Service::AudioBackendService* m_backend_service { nullptr };
};

} // namespace MayaFlux::Kakshya
