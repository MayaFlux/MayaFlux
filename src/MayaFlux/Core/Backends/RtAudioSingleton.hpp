#pragma once
#include "config.h"

namespace MayaFlux::Core {

class RtAudioSingleton {
private:
    static std::unique_ptr<RtAudio> s_instance;
    static std::mutex s_mutex;
    static bool s_stream_open;

    RtAudioSingleton() = default;

public:
    static RtAudio* get_instance()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_instance) {
            s_instance = std::make_unique<RtAudio>();
        }
        return s_instance.get();
    }

    static void mark_stream_open()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_stream_open) {
            throw std::runtime_error("Error: Attempted to open a second RtAudio stream when one is already open");
        }
        s_stream_open = true;
    }

    static void mark_stream_closed()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stream_open = false;
    }

    static bool is_stream_open()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_stream_open;
    }

    static void cleanup()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_instance && s_stream_open) {
            try {
                if (s_instance->isStreamRunning()) {
                    s_instance->stopStream();
                }
                if (s_instance->isStreamOpen()) {
                    s_instance->closeStream();
                }
                s_stream_open = false;
            } catch (const RtAudioErrorType& e) {
                std::cerr << "Error during RtAudio cleanup: " << s_instance->getErrorText() << std::endl;
            }
        }
        if (s_instance) {
            s_instance.reset();
        }
    }
};

}
