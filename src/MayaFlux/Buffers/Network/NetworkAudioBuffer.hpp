#pragma once

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/BufferProcessor.hpp"

namespace MayaFlux::Nodes::Network {
class NodeNetwork;
}

namespace MayaFlux::Buffers {

/**
 * @class NetworkAudioProcessor
 * @brief Processor that bridges NodeNetwork batch output into an AudioBuffer
 *
 * NetworkAudioProcessor drives or reads the batch output from a NodeNetwork
 * into an AudioBuffer. If the network has already been processed this cycle
 * (e.g. by NodeGraphManager during AUDIO_RATE processing), the processor
 * reads the cached result. If not, the processor drives process_batch()
 * itself, making the buffer work regardless of whether the network is
 * registered with NodeGraphManager.
 *
 * This mirrors the guard pattern used by NodeGraphManager:
 * check is_processed_this_cycle(), then mark_processing/process_batch/
 * mark_processed. If another context is currently processing, the processor
 * returns immediately rather than blocking.
 *
 * The processor respects the network's routing state, applying per-channel
 * scaling when the network is undergoing a routing transition. The mix
 * parameter controls interpolation between existing buffer content and
 * incoming network output.
 *
 * Processing sequence per cycle:
 * 1. Validate network has AUDIO_SINK or AUDIO_COMPUTE output mode
 * 2. Optionally clear buffer (clear_before_process)
 * 3. Ensure network is initialized
 * 4. Drive process_batch() if not yet processed this cycle
 * 5. Read cached batch output via get_audio_buffer()
 * 6. Apply routing scale for the buffer's channel if active
 * 7. Mix into buffer data with configured mix level
 */
class MAYAFLUX_API NetworkAudioProcessor : public BufferProcessor {
public:
    /**
     * @brief Creates a processor connected to a NodeNetwork
     * @param network Source network whose batch output fills this buffer (must be AUDIO_SINK or AUDIO_COMPUTE)
     * @param mix Interpolation coefficient between existing and incoming data (0.0-1.0)
     * @param clear_before_process Whether to zero the buffer before writing network output
     */
    NetworkAudioProcessor(
        std::shared_ptr<Nodes::Network::NodeNetwork> network,
        float mix = 1.0F,
        bool clear_before_process = true);

    /**
     * @brief Drives or reads network batch output into the buffer
     * @param buffer Target buffer to fill with network batch data
     *
     * If the network has not been processed this cycle, drives process_batch()
     * with the buffer's sample count. If another context is currently processing,
     * returns immediately without writing data.
     */
    void processing_function(const std::shared_ptr<Buffer>& buffer) override;

    void set_mix(float mix) { m_mix = mix; }
    [[nodiscard]] float get_mix() const { return m_mix; }

    void on_attach(const std::shared_ptr<Buffer>& buffer) override;
    void on_detach(const std::shared_ptr<Buffer>& buffer) override;

private:
    std::shared_ptr<Nodes::Network::NodeNetwork> m_network;
    float m_mix;
    bool m_clear_before_process;

    bool m_self_processing {};
};

/**
 * @class NetworkAudioBuffer
 * @brief AudioBuffer that captures batch output from a NodeNetwork each cycle
 *
 * NetworkAudioBuffer provides a bridge between NodeNetwork (which produces batch
 * audio via process_batch()) and the AudioBuffer system (which participates
 * in buffer processing chains, pipelines, and capture operations).
 *
 * This is the network-level analogue of NodeBuffer. Where NodeBuffer drives
 * a single Node sample-by-sample through save_state/process_sample/restore_state,
 * NetworkAudioBuffer drives or reads the network's batch result. The network may
 * or may not be managed by NodeGraphManager; NetworkAudioBuffer handles both cases.
 *
 * The network must have OutputMode::AUDIO_SINK or AUDIO_COMPUTE. Construction
 * with a non-audio network is a fatal error.
 *
 * The buffer does NOT register itself on any audio output channel. It exists
 * as an independent capture point. The network's normal output path through
 * RootAudioBuffer/ChannelProcessor is unaffected.
 */
class MAYAFLUX_API NetworkAudioBuffer : public AudioBuffer {
public:
    /**
     * @brief Creates a buffer connected to a NodeNetwork
     * @param channel_id Channel identifier for this buffer
     * @param num_samples Buffer size in samples (should match system buffer size)
     * @param network Source network whose batch output populates this buffer (must be AUDIO_SINK or AUDIO_COMPUTE)
     * @param clear_before_process Whether to zero buffer before each cycle
     */
    NetworkAudioBuffer(
        uint32_t channel_id,
        uint32_t num_samples,
        std::shared_ptr<Nodes::Network::NodeNetwork> network,
        bool clear_before_process = true);

    void setup_processors(ProcessingToken token) override;

    void set_clear_before_process(bool value) { m_clear_before_process = value; }
    [[nodiscard]] bool get_clear_before_process() const { return m_clear_before_process; }

    void process_default() override;

    [[nodiscard]] std::shared_ptr<Nodes::Network::NodeNetwork> get_network() const { return m_network; }

protected:
    std::shared_ptr<BufferProcessor> create_default_processor() override;

private:
    std::shared_ptr<Nodes::Network::NodeNetwork> m_network;
    bool m_clear_before_process;
};

}
