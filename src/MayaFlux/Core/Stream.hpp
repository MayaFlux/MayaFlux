#pragma once

#include "config.h"

namespace MayaFlux::Core {

class Engine;

struct GlobalStreamInfo;

class Stream {
public:
    Stream(unsigned int out_device, Engine* engine);
    ~Stream();

    void open();
    void start();
    void stop();
    void close();

    bool is_running() const;
    bool is_open() const;

private:
    RtAudio::StreamParameters m_Parameters;
    RtAudio::StreamOptions m_Options;

    bool m_is_open;
    bool m_is_running;

    void handle_stream_error(RtAudioErrorType error);

    Engine* m_engine;
};
}
