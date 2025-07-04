#pragma once

#include "Subsystem.hpp"

#include "MayaFlux/Core/Backends/AudioBackend/AudioBackend.hpp"

namespace MayaFlux::Core {

class Engine;

class AudioSubsystem : public ISubsystem {
public:
    virtual ~AudioSubsystem() = default;

    virtual void initialize(std::shared_ptr<Nodes::NodeGraphManager> node_graph_manager, std::shared_ptr<Buffers::BufferManager> buffer_manager) override;

    virtual void register_callbacks() override;

    virtual void start() override;

    virtual void stop() override;

    virtual void shutdown() override;

    inline virtual SubsystemTokens get_subsystem_tokens() const override { return m_subsystem_tokens; }

    inline virtual bool is_ready() const override { return m_is_ready; }

    inline IAudioBackend* get_audio_backend() { return m_audiobackend.get(); }
    inline const AudioStream* get_stream_manager() const { return m_audio_stream.get(); }
    inline const AudioDevice* get_device_manager() const { return m_audio_device.get(); }
    inline const GlobalStreamInfo& get_stream_info() const { return m_stream_info; }

    /**
     * @brief Processes input data
     * @param input_buffer Pointer to input data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     */
    int process_input(double* input_buffer, unsigned int num_frames);

    /**
     * @brief Processes output data
     * @param output_buffer Pointer to output data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     *
     * Processes scheduled tasks and fills the output buffer with processed data.
     */
    int process_output(double* output_buffer, unsigned int num_frames);

    /**
     * @brief Processes both input and output data
     * @param input_buffer Pointer to input data buffer
     * @param output_buffer Pointer to output data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     */
    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

private:
    friend class Engine;

    explicit AudioSubsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type = Utils::AudioBackendType::RTAUDIO);

    int audio_callback(void* output_buffer, void* input_buffer, unsigned int num_frames);

    void register_token_processors();

    GlobalStreamInfo m_stream_info;

    std::unique_ptr<IAudioBackend> m_audiobackend;
    std::unique_ptr<AudioDevice> m_audio_device;
    std::unique_ptr<AudioStream> m_audio_stream;

    std::shared_ptr<MayaFlux::Nodes::NodeGraphManager> m_node_graph_manager;
    std::shared_ptr<MayaFlux::Buffers::BufferManager> m_buffer_manager;

    SubsystemTokens m_subsystem_tokens;

    bool m_is_ready;
    bool m_is_running;
};
}
