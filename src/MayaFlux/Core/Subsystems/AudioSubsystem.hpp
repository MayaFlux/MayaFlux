#pragma once

#include "Subsystem.hpp"

#include "MayaFlux/Core/Backends/AudioBackend/AudioBackend.hpp"

namespace MayaFlux::Core {

/**
 * @class AudioSubsystem
 * @brief Audio processing subsystem managing real-time audio I/O and processing
 *
 * Implements the ISubsystem interface to provide audio-specific processing capabilities
 * within the MayaFlux engine. Manages audio backends, devices, and streams while
 * coordinating with the token-based processing architecture for buffer, node and scheduling operations.
 *
 * Uses AUDIO_BACKEND token for buffer processing and AUDIO_RATE token for node processing,
 * enabling real-time audio processing with proper thread safety and resource isolation.
 */
class AudioSubsystem : public ISubsystem {
public:
    virtual ~AudioSubsystem() = default;

    /** @brief Initialize audio processing with provided handle */
    virtual void initialize(SubsystemProcessingHandle& handle) override;

    /** @brief Register audio backend callbacks for real-time processing */
    virtual void register_callbacks() override;

    /** @brief Start audio processing and streaming */
    virtual void start() override;

    /** @brief Stop audio processing and streaming */
    virtual void stop() override;

    /** @brief Shutdown and cleanup audio resources */
    virtual void shutdown() override;

    /** @brief Get audio subsystem token configuration */
    inline virtual SubsystemTokens get_tokens() const override { return m_subsystem_tokens; }

    /** @brief Check if audio subsystem is ready for operation */
    inline virtual bool is_ready() const override { return m_is_ready; }

    /** @brief Check if audio subsystem is currently running */
    inline virtual bool is_running() const override { return m_is_running; }

    /** @brief Get access to the underlying audio backend */
    inline IAudioBackend* get_audio_backend() { return m_audiobackend.get(); }

    /** @brief Get read-only access to stream manager */
    inline const AudioStream* get_stream_manager() const { return m_audio_stream.get(); }

    /** @brief Get read-only access to device manager */
    inline const AudioDevice* get_device_manager() const { return m_audio_device.get(); }

    /** @brief Get global stream configuration */
    inline const GlobalStreamInfo& get_stream_info() const { return m_stream_info; }

    /**
     * @brief Processes input data from audio interface
     * @param input_buffer Pointer to interleaved input data
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     *
     * Handles incoming audio data from the audio interface, converting from
     * interleaved format and routing to appropriate buffer channels for processing.
     */
    int process_input(double* input_buffer, unsigned int num_frames);

    /**
     * @brief Processes output data for audio interface
     * @param output_buffer Pointer to interleaved output buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     *
     * Processes node graph and buffer operations, then fills the output buffer
     * with processed audio data in interleaved format for the audio interface.
     * This is the main processing entry point called by audio callbacks.
     */
    int process_output(double* output_buffer, unsigned int num_frames);

    /**
     * @brief Processes both input and output data in full-duplex mode
     * @param input_buffer Pointer to input data buffer
     * @param output_buffer Pointer to output data buffer
     * @param num_frames Number of frames to process
     * @return Status code (0 for success)
     *
     * Handles full-duplex audio processing, processing input and generating
     * output simultaneously. Used for real-time effects and monitoring scenarios.
     */
    int process_audio(double* input_buffer, double* output_buffer, unsigned int num_frames);

    /**
     * @brief Constructs AudioSubsystem with stream configuration
     * @param stream_info Global stream configuration
     * @param backend_type Audio backend type to use (default: RTAUDIO)
     *
     * Constructor - AudioSubsystem instances are created by Engine.
     * Initializes audio backend and configures processing tokens.
     */
    explicit AudioSubsystem(GlobalStreamInfo& stream_info, Utils::AudioBackendType backend_type = Utils::AudioBackendType::RTAUDIO);

    inline virtual SubsystemType get_type() const override { return m_type; }

    virtual SubsystemProcessingHandle* get_processing_context_handle() override { return m_handle; }

private:
    GlobalStreamInfo m_stream_info; ///< Audio stream configuration

    std::unique_ptr<IAudioBackend> m_audiobackend; ///< Audio backend implementation
    std::unique_ptr<AudioDevice> m_audio_device; ///< Audio device manager
    std::unique_ptr<AudioStream> m_audio_stream; ///< Audio stream manager

    SubsystemTokens m_subsystem_tokens; ///< Processing token configuration
    SubsystemProcessingHandle* m_handle; ///< Reference to processing handle

    bool m_is_ready; ///< Subsystem ready state
    bool m_is_running; ///< Subsystem running state

    static const SubsystemType m_type = SubsystemType::AUDIO;
};

}
