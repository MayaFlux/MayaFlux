#pragma once

#include "MayaFlux/Core/GlobalStreamInfo.hpp"
#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Core {
class AudioDevice;
class AudioStream;

/**
 * @struct DeviceInfo
 * @brief Contains digital audio device configuration parameters
 *
 * Encapsulates the technical specifications and capabilities of an audio I/O endpoint,
 * including channel counts, sample rate capabilities, and system identification.
 */
struct DeviceInfo {
    /** @brief System identifier for the audio endpoint */
    std::string name;

    /** @brief Number of discrete input channels available for signal capture */
    uint32_t input_channels = 0;

    /** @brief Number of discrete output channels available for signal playback */
    uint32_t output_channels = 0;

    /** @brief Number of channels supporting simultaneous input and output */
    uint32_t duplex_channels = 0;

    /** @brief Optimal sample rate for this device as reported by the system */
    uint32_t preferred_sample_rate = 0;

    /** @brief Collection of all sample rates supported by this device */
    std::vector<uint32_t> supported_samplerates;

    /** @brief Indicates if this device is the system's primary output endpoint */
    bool is_default_output = false;

    /** @brief Indicates if this device is the system's primary input endpoint */
    bool is_default_input = false;
};

/**
 * @class IAudioBackend
 * @brief Interface for audio system abstraction layer
 *
 * Defines the contract for platform-specific audio subsystem implementations,
 * providing hardware-agnostic access to digital audio I/O capabilities.
 * Implementations handle the translation between the engine's signal processing
 * domain and the system's audio infrastructure.
 */
class IAudioBackend {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~IAudioBackend() = default;

    /**
     * @brief Creates a device manager for audio endpoint discovery
     * @return Unique pointer to an AudioDevice implementation
     *
     * Instantiates a platform-specific device manager that can enumerate
     * and provide information about available audio endpoints.
     */
    virtual std::unique_ptr<AudioDevice> create_device_manager() = 0;

    /**
     * @brief Creates an audio stream for a specific device
     * @param output_device_id System identifier for the target audio output device
     * @param input_device_id System identifier for the target audio input device
     * @param stream_info Configuration parameters for the audio stream
     * @param user_data Optional context pointer passed to callbacks
     * @return Unique pointer to an AudioStream implementation
     *
     * Establishes a digital audio pipeline between the application and
     * the specified hardware endpoint with the requested configuration.
     */
    virtual std::unique_ptr<AudioStream> create_stream(
        unsigned int output_device_id,
        unsigned int input_device_id,
        GlobalStreamInfo& stream_info,
        void* user_data)
        = 0;

    /**
     * @brief Retrieves the backend implementation version
     * @return String representation of the backend version
     */
    [[nodiscard]] virtual std::string get_version_string() const = 0;

    /**
     * @brief Retrieves the backend API type identifier
     * @return Integer code representing the backend type
     */
    [[nodiscard]] virtual int get_api_type() const = 0;

    /**
     * @brief Releases all resources held by the backend.
     *
     * Performs necessary cleanup operations before backend destruction,
     * including releasing system resources and terminating background processes.
     * This method should be called only before application termination to
     * ensure proper resource deallocation and prevent memory leaks. It is not
     * intended for general use and should not be called during normal
     * application operation.
     * NOTE: The nature of unique_ptr makes manual cleanup redundant. This only exists when it is necessary to
     * take ownership of the cleanup, for example, when a different backend needs to be used
     * or when Audio is no longer needed.
     */
    virtual void cleanup() = 0;
};

/**
 * @class AudioDevice
 * @brief Manages audio endpoint discovery and enumeration
 *
 * Provides access to the system's available audio I/O endpoints,
 * abstracting the platform-specific device enumeration process and
 * exposing a consistent interface for querying device capabilities.
 */
class AudioDevice {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~AudioDevice() = default;

    /**
     * @brief Retrieves information about all available output devices
     * @return Vector of DeviceInfo structures for output endpoints
     */
    [[nodiscard]] virtual std::vector<DeviceInfo> get_output_devices() const = 0;

    /**
     * @brief Retrieves information about all available input devices
     * @return Vector of DeviceInfo structures for input endpoints
     */
    [[nodiscard]] virtual std::vector<DeviceInfo> get_input_devices() const = 0;

    /**
     * @brief Gets the system's primary output device identifier
     * @return Device ID for the default output endpoint
     */
    [[nodiscard]] virtual unsigned int get_default_output_device() const = 0;

    /**
     * @brief Gets the system's primary input device identifier
     * @return Device ID for the default input endpoint
     */
    [[nodiscard]] virtual unsigned int get_default_input_device() const = 0;
};

/**
 * @class AudioStream
 * @brief Manages digital audio data flow between the engine and hardware
 *
 * Handles the real-time bidirectional transfer of audio sample data between
 * the processing engine and system audio endpoints. Provides lifecycle management
 * for audio streams and callback-based processing integration.
 */
class AudioStream {
public:
    /** @brief Virtual destructor for proper cleanup of derived classes */
    virtual ~AudioStream() = default;

    /**
     * @brief Initializes the audio stream and allocates required resources
     *
     * Prepares the stream for data transfer by establishing connections
     * to the hardware and allocating necessary buffers, but does not start
     * the actual data flow.
     */
    virtual void open() = 0;

    /**
     * @brief Activates the audio stream and begins data transfer
     *
     * Initiates the real-time processing of audio data, enabling the flow
     * of samples between the application and hardware endpoints.
     */
    virtual void start() = 0;

    /**
     * @brief Deactivates the audio stream and halts data transfer
     *
     * Suspends the real-time processing of audio data while maintaining
     * the stream's configuration and resource allocations.
     */
    virtual void stop() = 0;

    /**
     * @brief Terminates the audio stream and releases all resources
     *
     * Completely shuts down the audio stream, releasing all allocated
     * resources and disconnecting from hardware endpoints.
     */
    virtual void close() = 0;

    /**
     * @brief Checks if the stream is actively processing audio data
     * @return True if the stream is currently active, false otherwise
     */
    [[nodiscard]] virtual bool is_running() const = 0;

    /**
     * @brief Checks if the stream is initialized and ready for activation
     * @return True if the stream is open, false otherwise
     */
    [[nodiscard]] virtual bool is_open() const = 0;

    /**
     * @brief Sets the function to process audio data
     * @param processCallback Function that handles audio data processing
     *
     * Registers a callback function that will be invoked by the audio system
     * when new input data is available or output data is needed. The callback
     * receives pointers to input and output buffers along with the frame count.
     */
    virtual void set_process_callback(std::function<int(void*, void*, unsigned int)> processCallback) = 0;
};

/**
 * @class AudioBackendFactory
 * @brief Factory pattern implementation for audio backend instantiation
 *
 * Creates concrete implementations of the IAudioBackend interface based on
 * the requested backend type. Centralizes backend creation logic and enables
 * runtime selection of appropriate audio subsystem implementations.
 */
class AudioBackendFactory {
public:
    /**
     * @brief Creates a specific audio backend implementation
     * @param type Identifier for the requested backend type
     * @param api_preference Optional preference for a specific audio API
     * @return Unique pointer to an IAudioBackend implementation
     *
     * Instantiates and configures the appropriate backend implementation
     * based on the specified type, abstracting the details of backend
     * selection and initialization.
     */
    static std::unique_ptr<IAudioBackend> create_backend(
        Core::AudioBackendType type,
        std::optional<GlobalStreamInfo::AudioApi> api_preference = std::nullopt);
};
}
