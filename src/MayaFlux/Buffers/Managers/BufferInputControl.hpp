#pragma once

namespace MayaFlux::Buffers {

class AudioBuffer;
class InputAudioBuffer;

/**
 * @class BufferInputControl
 * @brief Audio input buffer management and listener coordination
 *
 * Manages all operations related to audio input handling: input buffer creation,
 * input data processing, and listener registration/unregistration.
 *
 * Design Principles:
 * - Owns input buffers: Manages the lifecycle of input buffer storage
 * - Listener coordination: Handles registration/unregistration of buffers listening to input
 * - Single responsibility: Only handles input-specific operations
 * - Extensible: Can be extended to handle other input types (MIDI, video capture, etc.)
 *
 * This class encapsulates all input-related logic that was previously scattered
 * throughout BufferManager.
 */
class MAYAFLUX_API BufferInputControl {
public:
    /**
     * @brief Creates a new input control handler
     */
    BufferInputControl() = default;

    ~BufferInputControl() = default;

    // =========================================================================
    // Input Buffer Lifecycle
    // =========================================================================

    /**
     * @brief Sets up audio input buffers
     * @param num_channels Number of input channels to create
     * @param buffer_size Block size the engine runs at
     *
     * Creates and initializes input buffers for each channel. This is typically
     * called during BufferManager initialization or when input channels change.
     * AudioBuffer takes its length from the engine's negotiated block size, so
     * @p buffer_size should be that value; the buffers are that length whatever
     * is passed.
     */
    void setup_audio_input_buffers(uint32_t num_channels, uint32_t buffer_size);

    /**
     * @brief Gets the number of audio input channels
     * @return Number of input channels, or 0 if not set up
     */
    uint32_t get_audio_input_channel_count() const;

    /**
     * @brief Gets the input buffer of a channel
     * @param channel Input channel index
     * @return The channel's input buffer, or nullptr if the channel does not exist
     */
    [[nodiscard]] std::shared_ptr<InputAudioBuffer> get_input_buffer(uint32_t channel) const;

    // =========================================================================
    // Input Data Processing
    // =========================================================================

    /**
     * @brief Processes incoming audio input data into input buffers
     * @param input_data Pointer to interleaved input data
     * @param num_channels Number of channels in the input data
     * @param num_frames Number of frames to process
     *
     * Takes interleaved input data and distributes it to the appropriate
     * input buffers, then triggers default processing on each. A buffer
     * longer than @p num_frames, or for a channel the data does not carry, is
     * zero-filled past what was read. Input never reads beyond @p num_frames
     * frames of @p num_channels channels. Fewer buffers than channels rebuilds
     * the buffers at @p num_frames samples.
     */
    void process_audio_input(double* input_data, uint32_t num_channels, uint32_t num_frames);

    // =========================================================================
    // Listener Management
    // =========================================================================

    /**
     * @brief Registers a buffer as a listener to an input channel
     * @param buffer Buffer to register as listener
     * @param channel Input channel to listen to
     *
     * The buffer will receive copies of input data from the specified channel
     * whenever process_audio_input() is called.
     */
    void register_audio_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel);

    /**
     * @brief Unregisters a buffer from an input channel
     * @param buffer Buffer to unregister
     * @param channel Input channel to stop listening to
     */
    void unregister_audio_input_listener(const std::shared_ptr<AudioBuffer>& buffer, uint32_t channel);

private:
    /// Input buffers for capturing audio input data
    std::vector<std::shared_ptr<InputAudioBuffer>> m_audio_input_buffers;
};

} // namespace MayaFlux::Buffers
