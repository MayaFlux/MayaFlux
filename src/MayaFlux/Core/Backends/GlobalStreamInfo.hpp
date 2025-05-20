#pragma once
#include "config.h"

namespace MayaFlux::Core {

struct GlobalStreamInfo {
    u_int32_t sample_rate = 48000;
    u_int32_t buffer_size = 512;

    enum class AudioFormat {
        FLOAT32,
        FLOAT64,
        INT16,
        INT24,
        INT32
    };
    AudioFormat format = AudioFormat::FLOAT64;
    bool non_interleaved = false;

    struct ChannelConfig {
        bool enabled = true;
        u_int32_t channels = 2;
        int device_id = -1;
        std::string device_name;
    };

    ChannelConfig output;
    ChannelConfig input = { false, 2, -1, "" };

    enum class StreamPriority {
        LOW,
        NORMAL,
        HIGH,
        REALTIME
    };
    StreamPriority priority = StreamPriority::REALTIME;
    double buffer_count = 0.f;

    bool auto_convert_format = true;
    bool handle_xruns = true;
    bool use_callback = true;
    double stream_latency_ms = 0.0;

    enum class DitherMethod {
        NONE,
        RECTANGULAR,
        TRIANGULAR,
        GAUSSIAN,
        SHAPED
    };
    DitherMethod dither = DitherMethod::NONE;

    struct MidiConfig {
        bool enabled = false;
        int device_id = -1;
    };

    MidiConfig midi_input;
    MidiConfig midi_output;

    bool measure_latency = false;
    bool verbose_logging = false;

    std::unordered_map<std::string, std::any> backend_options;

    u_int32_t get_total_channels() const
    {
        return (output.enabled ? output.channels : 0) + (input.enabled ? input.channels : 0);
    }

    u_int32_t get_num_channels() const { return output.channels; }
};

}
