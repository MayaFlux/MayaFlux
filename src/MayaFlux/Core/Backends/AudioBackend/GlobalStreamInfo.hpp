#pragma once

namespace MayaFlux::Core {

/**
 * @struct GlobalStreamInfo
 * @brief Comprehensive configuration for digital audio stream processing
 *
 * Defines the technical parameters and operational characteristics for
 * audio data flow throughout the system, including format specifications,
 * buffer configurations, and I/O endpoint settings.
 */
struct GlobalStreamInfo {
    /** @brief Number of samples processed per second (Hz) */
    u_int32_t sample_rate = 48000;

    /** @brief Number of samples per processing block */
    u_int32_t buffer_size = 512;

    /**
     * @enum AudioFormat
     * @brief Defines the binary representation of audio sample data
     *
     * Specifies the numerical precision and memory layout for digital
     * audio samples throughout the processing chain.
     */
    enum class AudioFormat {
        FLOAT32, ///< 32-bit floating point representation (-1.0 to 1.0)
        FLOAT64, ///< 64-bit floating point representation (-1.0 to 1.0)
        INT16, ///< 16-bit integer representation (-32768 to 32767)
        INT24, ///< 24-bit integer representation (-8388608 to 8388607)
        INT32 ///< 32-bit integer representation (-2147483648 to 2147483647)
    };

    /** @brief Sample data format for stream processing */
    AudioFormat format = AudioFormat::FLOAT64;

    /** @brief Channel organization mode (true: planar, false: interleaved) */
    bool non_interleaved = false;

    /**
     * @struct ChannelConfig
     * @brief Configuration for input or output data channels
     *
     * Defines the parameters for a set of audio channels that either
     * capture input signals or render output signals.
     */
    struct ChannelConfig {
        /** @brief Whether this channel set is active in the stream */
        bool enabled = true;

        /** @brief Number of discrete channels in this set */
        u_int32_t channels = 2;

        /** @brief System identifier for the associated device (-1 for default) */
        int device_id = -1;

        /** @brief Human-readable identifier for the associated device */
        std::string device_name;
    };

    /** @brief Configuration for output signal channels */
    ChannelConfig output;

    /** @brief Configuration for input signal channels (disabled by default) */
    ChannelConfig input = { false, 2, -1, "" };

    /**
     * @enum StreamPriority
     * @brief Processing priority levels for the audio stream
     *
     * Defines the system resource allocation priority for the audio
     * processing thread relative to other system processes.
     */
    enum class StreamPriority {
        LOW, ///< Minimal resource priority
        NORMAL, ///< Standard resource priority
        HIGH, ///< Elevated resource priority
        REALTIME ///< Maximum resource priority with timing guarantees
    };

    /** @brief System resource priority for audio processing */
    StreamPriority priority = StreamPriority::REALTIME;

    /** @brief Number of buffers in the processing queue (0 for system default) */
    double buffer_count = 0.f;

    /** @brief Whether to automatically convert between sample formats */
    bool auto_convert_format = true;

    /** @brief Whether to handle buffer underrun/overrun conditions */
    bool handle_xruns = true;

    /** @brief Whether to use callback-based processing (vs. blocking I/O) */
    bool use_callback = true;

    /** @brief Target latency for stream processing in milliseconds */
    double stream_latency_ms = 0.0;

    /**
     * @enum DitherMethod
     * @brief Noise shaping algorithms for quantization error mitigation
     *
     * Defines the mathematical approach used to distribute quantization
     * errors when converting between different sample formats.
     */
    enum class DitherMethod {
        NONE, ///< No dithering applied
        RECTANGULAR, ///< Uniform random distribution
        TRIANGULAR, ///< Weighted triangular distribution
        GAUSSIAN, ///< Normal (bell curve) distribution
        SHAPED ///< Psychoacoustically optimized noise shaping
    };

    /** @brief Dithering algorithm for format conversions */
    DitherMethod dither = DitherMethod::NONE;

    /**
     * @struct MidiConfig
     * @brief Configuration for MIDI data channels
     *
     * Defines the parameters for digital musical instrument control
     * data transmission and reception.
     */
    struct MidiConfig {
        /** @brief Whether this MIDI channel is active */
        bool enabled = false;

        /** @brief System identifier for the associated MIDI device (-1 for default) */
        int device_id = -1;
    };

    /** @brief Configuration for MIDI input data */
    MidiConfig midi_input;

    /** @brief Configuration for MIDI output data */
    MidiConfig midi_output;

    /** @brief Whether to measure and report actual stream latency */
    bool measure_latency = false;

    /** @brief Whether to output detailed diagnostic information */
    bool verbose_logging = false;

    /** @brief Backend-specific configuration parameters */
    std::unordered_map<std::string, std::any> backend_options;

    /**
     * @brief Calculates the total number of active channels across all directions
     * @return Sum of all enabled input and output channels
     */
    u_int32_t get_total_channels() const
    {
        return (output.enabled ? output.channels : 0) + (input.enabled ? input.channels : 0);
    }

    /**
     * @brief Retrieves the number of output channels
     * @return Number of output channels configured in the stream
     */
    u_int32_t get_num_channels() const { return output.channels; }
};

}
