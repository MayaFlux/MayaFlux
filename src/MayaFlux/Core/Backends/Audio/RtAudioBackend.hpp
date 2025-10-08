#pragma once
#include "AudioBackend.hpp"

#ifdef RTAUDIO_BACKEND
#include "RtAudio.h"
#endif

namespace MayaFlux::Core {

/**
 * @brief Converts RtAudio-specific device information to the engine's device model
 * @param rtInfo Native RtAudio device information structure
 * @param id System identifier for the device
 * @param defaultOutputDevice System identifier for the default output device
 * @param defaultInputDevice System identifier for the default input device
 * @return Standardized DeviceInfo structure with platform-agnostic representation
 *
 * Maps the vendor-specific device information format to the engine's
 * standardized representation, enabling consistent device handling
 * across different audio backends.
 */
// TODO:: Need to see if this is truly necessary or if DeviceInfo can be more accommodating
static DeviceInfo convert_device_info(
    const RtAudio::DeviceInfo& rtInfo,
    unsigned int id,
    unsigned int defaultOutputDevice,
    unsigned int defaultInputDevice)
{
    DeviceInfo info;
    info.name = rtInfo.name;
    info.input_channels = rtInfo.inputChannels;
    info.output_channels = rtInfo.outputChannels;
    info.duplex_channels = rtInfo.duplexChannels;
    info.preferred_sample_rate = rtInfo.preferredSampleRate;
    info.supported_samplerates = rtInfo.sampleRates;
    info.is_default_output = (id == defaultOutputDevice);
    info.is_default_input = (id == defaultInputDevice);
    return info;
}

/**
 * @class RtAudioBackend
 * @brief RtAudio implementation of the audio backend interface
 *
 * Provides a concrete implementation of the IAudioBackend interface
 * using the RtAudio cross-platform audio I/O library. This class
 * manages the lifecycle of the RtAudio context and serves as a factory
 * for RtAudio-specific device and stream implementations.
 */
class RtAudioBackend : public IAudioBackend {
public:
    /**
     * @brief Initializes the RtAudio backend
     *
     * Creates and configures the RtAudio context with default settings.
     * The backend is ready for device enumeration and stream creation
     * immediately after construction.
     */
    RtAudioBackend();

    /**
     * @brief Cleans up the RtAudio backend
     *
     * Ensures proper release of all RtAudio resources when the backend
     * is no longer needed.
     */
    ~RtAudioBackend() override;

    /**
     * @brief Creates an RtAudio-specific device manager
     * @return Unique pointer to an RtAudioDevice implementation
     *
     * Instantiates a device manager that uses the RtAudio API to
     * enumerate and provide information about available audio endpoints.
     */
    std::unique_ptr<AudioDevice> create_device_manager() override;

    /**
     * @brief Creates an RtAudio-specific audio stream
     * @param output_device_id System identifier for the target audio output device
     * @param input_device_id System identifier for the target audio input device
     * @param stream_info Configuration parameters for the audio stream
     * @param user_data Optional context pointer passed to callbacks
     * @return Unique pointer to an RtAudioStream implementation
     *
     * Establishes a digital audio pipeline between the application and
     * the specified hardware endpoint using the RtAudio API.
     */
    std::unique_ptr<AudioStream> create_stream(
        unsigned int output_device_id,
        unsigned int input_device_id,
        const GlobalStreamInfo& stream_info,
        void* user_data) override;

    /**
     * @brief Retrieves the RtAudio library version
     * @return String representation of the RtAudio version
     */
    [[nodiscard]] std::string get_version_string() const override;

    /**
     * @brief Retrieves the RtAudio API type identifier
     * @return Integer code representing the active audio API
     *
     * Returns a value from the RtAudio::Api enumeration indicating
     * which underlying audio system is being used (WASAPI, ALSA, etc.).
     */
    [[nodiscard]] int get_api_type() const override;

    /**
     * @brief Provides safe access to the RtAudio context
     * @return Reference to the underlying RtAudio instance
     *
     * Exposes the native RtAudio handle for operations that require
     * direct access to the RtAudio API.
     */
    RtAudio& get_context_reference() { return *m_context; }

    /**
     * @brief Releases all resources held by the backend
     *
     * Performs necessary cleanup operations before backend destruction,
     * including releasing the RtAudio context and any associated resources.
     * This method should be called only before application termination to
     * ensure proper resource deallocation and prevent memory leaks. It is not
     * intended for general use and should not be called during normal
     * application operation.
     * NOTE: The nature of unique_ptr makes manual cleanup redundant. This only exists when it is necessary to
     * take ownership of the cleanup, for example, when a different backend needs to be used
     * or when Audio is no longer needed.
     */
    void cleanup() override;

private:
    /** @brief Pointer to the underlying RtAudio context */
    RtAudio* m_context;
};

/**
 * @class RtAudioDevice
 * @brief RtAudio implementation of the audio device interface
 *
 * Provides device enumeration and information services using the
 * RtAudio API. Maintains cached device lists to improve performance
 * for repeated device queries.
 */
class RtAudioDevice : public AudioDevice {
public:
    /**
     * @brief Initializes the device manager with an RtAudio context
     * @param context Pointer to an active RtAudio instance
     *
     * Creates a device manager that uses the provided RtAudio context
     * to enumerate and query audio devices. The device lists are
     * populated during construction.
     */
    RtAudioDevice(RtAudio* context);

    /**
     * @brief Retrieves information about all available output devices
     * @return Vector of DeviceInfo structures for output endpoints
     */
    [[nodiscard]] std::vector<DeviceInfo> get_output_devices() const override;

    /**
     * @brief Retrieves information about all available input devices
     * @return Vector of DeviceInfo structures for input endpoints
     */
    [[nodiscard]] std::vector<DeviceInfo> get_input_devices() const override;

    /**
     * @brief Gets the system's primary output device identifier
     * @return Device ID for the default output endpoint
     */
    [[nodiscard]] unsigned int get_default_output_device() const override;

    /**
     * @brief Gets the system's primary input device identifier
     * @return Device ID for the default input endpoint
     */
    [[nodiscard]] unsigned int get_default_input_device() const override;

private:
    /** @brief Pointer to the underlying RtAudio context */
    RtAudio* m_context;

    /** @brief Cached list of output devices */
    std::vector<DeviceInfo> m_output_devices;

    /** @brief Cached list of input devices */
    std::vector<DeviceInfo> m_input_devices;

    /** @brief System identifier for the default output device */
    unsigned int m_defaultOutputDevice;

    /** @brief System identifier for the default input device */
    unsigned int m_defaultInputDevice;
};

/**
 * @class RtAudioStream
 * @brief RtAudio implementation of the audio stream interface
 *
 * Manages real-time audio data flow between the application and hardware
 * using the RtAudio API. Handles the configuration, lifecycle, and callback
 * integration for audio streams.
 */
class RtAudioStream : public AudioStream {
public:
    /**
     * @brief Initializes an audio stream with the specified configuration
     * @param context Pointer to an active RtAudio instance
     * @param output_device_id System identifier for the target audio output device
     * @param input_device_id System identifier for the target audio input device
     * @param streamInfo Configuration parameters for the audio stream
     * @param userData Optional context pointer passed to callbacks
     *
     * Creates an audio stream configuration but does not open the stream.
     * The stream must be explicitly opened with open() before use.
     */
    RtAudioStream(
        RtAudio* context,
        unsigned int output_device_id,
        unsigned int input_device_id,
        const GlobalStreamInfo& streamInfo,
        void* userData);

    /**
     * @brief Ensures proper cleanup of stream resources
     *
     * Automatically closes the stream if it's still open during destruction.
     */
    ~RtAudioStream() override;

    /**
     * @brief Initializes the audio stream and allocates required resources
     *
     * Opens the RtAudio stream with the configured parameters, making it
     * ready for data transfer. Does not start the actual data flow.
     */
    void open() override;

    /**
     * @brief Activates the audio stream and begins data transfer
     *
     * Starts the RtAudio stream, initiating the real-time processing
     * of audio data through the registered callback function.
     */
    void start() override;

    /**
     * @brief Deactivates the audio stream and halts data transfer
     *
     * Stops the RtAudio stream, suspending the real-time processing
     * while maintaining the stream's configuration.
     */
    void stop() override;

    /**
     * @brief Terminates the audio stream and releases all resources
     *
     * Closes the RtAudio stream, releasing all allocated resources
     * and disconnecting from hardware endpoints.
     */
    void close() override;

    /**
     * @brief Checks if the stream is actively processing audio data
     * @return True if the stream is currently active, false otherwise
     */
    bool is_running() const override;

    /**
     * @brief Checks if the stream is initialized and ready for activation
     * @return True if the stream is open, false otherwise
     */
    bool is_open() const override;

    /**
     * @brief Sets the function to process audio data
     * @param processCallback Function that handles audio data processing
     *
     * Registers a callback function that will be invoked by the RtAudio
     * system when new input data is available or output data is needed.
     */
    void set_process_callback(
        std::function<int(void*, void*, unsigned int)> processCallback) override;

private:
    /**
     * @brief Static callback function for the RtAudio API
     * @param output_buffer Pointer to the output sample buffer
     * @param input_buffer Pointer to the input sample buffer
     * @param num_frames Number of frames to process
     * @param stream_time Current stream time in seconds
     * @param status Stream status flags
     * @param user_data Pointer to the RtAudioStream instance
     * @return 0 on success, non-zero on error
     *
     * This static function serves as the entry point for the RtAudio
     * callback system. It unpacks the parameters and delegates to the
     * user-provided callback function registered with set_process_callback.
     */
    static int rtAudioCallback(
        void* output_buffer,
        void* input_buffer,
        unsigned int num_frames,
        double stream_time,
        RtAudioStreamStatus status,
        void* user_data);

    /** @brief Pointer to the underlying RtAudio context */
    RtAudio* m_context;

    /** @brief RtAudio-specific stream output configuration parameters */
    RtAudio::StreamParameters m_out_parameters;

    /** @brief RtAudio-specific stream input configuration parameters */
    RtAudio::StreamParameters m_in_parameters;

    /** @brief RtAudio-specific stream options */
    RtAudio::StreamOptions m_options;

    /** @brief Engine stream configuration */
    GlobalStreamInfo stream_info;

    /** @brief User-provided context pointer for callbacks */
    void* m_userData;

    /** @brief Flag indicating if the stream is currently open */
    bool m_isOpen;

    /** @brief Flag indicating if the stream is currently running */
    bool m_isRunning;

    /** @brief User-provided callback function for audio processing */
    std::function<int(void*, void*, unsigned int)> m_process_callback;

    /** @brief Copy of the stream configuration for reference */
    GlobalStreamInfo m_stream_info;

    /**
     * @brief Configures RtAudio stream options based on GlobalStreamInfo
     *
     * Maps the engine's stream configuration parameters to the
     * corresponding RtAudio-specific options.
     */
    void configure_stream_options();
};

} // namespace MayaFlux::Core
