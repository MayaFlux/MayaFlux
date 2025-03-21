#pragma once

#include "MayaFlux/MayaFlux.hpp"

#include "Device.hpp"

#include "Stream.hpp"

namespace MayaFlux {

namespace Core {

    class Engine {
    public:
        Engine();
        ~Engine() = default;

        void Init(GlobalStreamInfo stream_info);

        inline const std::shared_ptr<Stream> get_stream_settings() const
        {
            return m_StreamSettings;
        }

        // void Start();
        // void End();

    private:
        std::unique_ptr<RtAudio> m_Context;

        RtAudio* get_handle()
        {
            return m_Context.get();
        }

        std::shared_ptr<Device> m_Device;

        std::shared_ptr<Stream> m_StreamSettings;

        int Callback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
            double streamTime, RtAudioStreamStatus status, void* userData);
    };
}

}
