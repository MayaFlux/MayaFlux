#pragma once

#include "config.h"

namespace MayaFlux {

namespace Core {

    /**
     * @class Device
     * @brief Abstracts audio device management across different audio backends
     *
     * The Device class provides a unified interface for discovering and managing
     * audio input and output devices. It currently wraps RtAudio's device management
     * functionality, but is designed to be extensible to support multiple audio
     * backends in the future.
     *
     * This abstraction layer isolates the rest of the MayaFlux engine from the
     * specifics of the underlying audio API, making it easier to add support for
     * additional backends like JACK, ASIO, CoreAudio, or WASAPI without changing
     * the higher-level code.
     *
     * Example usage:
     * ```cpp
     * // Create an RtAudio context
     * auto rtaudio = std::make_unique<RtAudio>();
     *
     * // Create a device manager using that context
     * Device device(rtaudio.get());
     *
     * // Get the default output device ID
     * unsigned int output_device = device.get_default_output_device();
     *
     * // Get information about available output devices
     * for (const auto& device_info : device.get_output_devices()) {
     *     std::cout << "Device: " << device_info.name << std::endl;
     *     std::cout << "Max Output Channels: " << device_info.outputChannels << std::endl;
     * }
     * ```
     *
     * The Device class is a foundational component of the audio engine, providing
     * the necessary information for configuring audio streams and ensuring that
     * the engine can work with the available hardware.
     */
    class Device {
    public:
        /**
         * @brief Constructs a Device manager with the specified audio context
         * @param context Pointer to an RtAudio instance that will be used for device management
         *
         * This constructor initializes the Device manager with the provided RtAudio
         * context. It queries the available input and output devices and identifies
         * the default devices for both input and output.
         *
         * The context must remain valid for the lifetime of the Device instance.
         */
        Device(RtAudio* context);

        /**
         * @brief Default destructor
         *
         * The destructor does not perform any special cleanup, as the Device class
         * does not own the RtAudio context it was initialized with.
         */
        ~Device() = default;

        /**
         * @brief Gets the ID of the default output device
         * @return The device ID of the default output device
         *
         * This method returns the device ID of the system's default output device,
         * as reported by the audio backend. This ID can be used to open an audio
         * stream for output.
         *
         * The device ID is specific to the audio backend being used and should be
         * treated as an opaque identifier.
         */
        inline unsigned int get_default_output_device() const
        {
            return m_default_out_device;
        }

        /**
         * @brief Gets the ID of the default input device
         * @return The device ID of the default input device
         *
         * This method returns the device ID of the system's default input device,
         * as reported by the audio backend. This ID can be used to open an audio
         * stream for input.
         *
         * The device ID is specific to the audio backend being used and should be
         * treated as an opaque identifier.
         */
        inline unsigned int get_default_input_device() const
        {
            return m_default_in_device;
        }

        /**
         * @brief Gets information about all available input devices
         * @return A vector of DeviceInfo structures for input devices
         *
         * This method returns detailed information about all available input devices,
         * including their names, channel counts, supported sample rates, and other
         * capabilities.
         *
         * The information is provided as RtAudio::DeviceInfo structures, which contain
         * fields like name, inputChannels, outputChannels, duplexChannels, isDefaultInput,
         * isDefaultOutput, sampleRates, preferredSampleRate, and nativeFormats.
         */
        inline const std::vector<RtAudio::DeviceInfo>& get_input_devices() const
        {
            return m_Input_devices;
        }

        /**
         * @brief Gets information about all available output devices
         * @return A vector of DeviceInfo structures for output devices
         *
         * This method returns detailed information about all available output devices,
         * including their names, channel counts, supported sample rates, and other
         * capabilities.
         *
         * The information is provided as RtAudio::DeviceInfo structures, which contain
         * fields like name, inputChannels, outputChannels, duplexChannels, isDefaultInput,
         * isDefaultOutput, sampleRates, preferredSampleRate, and nativeFormats.
         */
        inline const std::vector<RtAudio::DeviceInfo>& get_output_devices() const
        {
            return m_Output_devices;
        }

    private:
        /**
         * @brief Number of available input devices
         *
         * This field stores the count of available input devices, as reported
         * by the audio backend during initialization.
         */
        unsigned int m_Num_in_devices;

        /**
         * @brief Number of available output devices
         *
         * This field stores the count of available output devices, as reported
         * by the audio backend during initialization.
         */
        unsigned int m_Num_out_devices;

        /**
         * @brief Detailed information about available input devices
         *
         * This vector contains DeviceInfo structures for all available input devices,
         * populated during initialization.
         */
        std::vector<RtAudio::DeviceInfo> m_Input_devices;

        /**
         * @brief Detailed information about available output devices
         *
         * This vector contains DeviceInfo structures for all available output devices,
         * populated during initialization.
         */
        std::vector<RtAudio::DeviceInfo> m_Output_devices;

        /**
         * @brief Device ID of the default output device
         *
         * This field stores the device ID of the system's default output device,
         * as reported by the audio backend during initialization.
         */
        unsigned int m_default_out_device;

        /**
         * @brief Device ID of the default input device
         *
         * This field stores the device ID of the system's default input device,
         * as reported by the audio backend during initialization.
         */
        unsigned int m_default_in_device;
    };
}

}
