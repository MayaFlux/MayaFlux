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
     * @param buffer_size Size of each input buffer in samples
     *
     * Creates and initializes input buffers for each channel. This is typically
     * called during BufferManager initialization or when input channels change.
     */
    void setup_audio_input_buffers(uint32_t num_channels, uint32_t buffer_size);

    /**
     * @brief Gets the number of audio input channels
     * @return Number of input channels, or 0 if not set up
     */
    uint32_t get_audio_input_channel_count() const;

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
     * input buffers, then triggers default processing on each.
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
