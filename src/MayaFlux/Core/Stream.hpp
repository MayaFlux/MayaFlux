#pragma once

#include "config.h"

namespace MayaFlux::Core {

class Engine;

struct GlobalStreamInfo {
    unsigned int sample_rate;
    unsigned int buffer_size;
    unsigned int num_channels;
};

class Stream {
public:
    Stream(unsigned int out_device, GlobalStreamInfo stream_info, Engine* engine);
    ~Stream();

    void open();
    void start();
    void stop();
    void close();

    bool is_running() const;
    bool is_open() const;

    inline GlobalStreamInfo get_global_stream_info()
    {
        return m_Stream_info;
    }

    inline RtAudio::StreamOptions get_stream_options()
    {
        return m_Options;
    }

    inline RtAudio::StreamParameters get_stream_parameters()
    {
        return m_Parameters;
    }

private:
    RtAudio::StreamParameters m_Parameters;
    RtAudio::StreamOptions m_Options;
    GlobalStreamInfo m_Stream_info;

    bool m_is_open;
    bool m_is_running;

    void handle_stream_error(RtAudioErrorType error);

    Engine* m_engine;
};
}
