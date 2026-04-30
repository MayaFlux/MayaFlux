#include "AudioBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#ifdef RTAUDIO_BACKEND
#include "RtAudioBackend.hpp"
#include "RtAudioSingleton.hpp"
#include <RtAudio.h>

namespace {
RtAudio::Api to_rtaudio_api(MayaFlux::Core::GlobalStreamInfo::AudioApi api)
{
    using Api = MayaFlux::Core::GlobalStreamInfo::AudioApi;
    switch (api) {
    case Api::ALSA:
        return RtAudio::LINUX_ALSA;
    case Api::PULSE:
        return RtAudio::LINUX_PULSE;
    case Api::JACK:
        return RtAudio::UNIX_JACK;
    case Api::CORE:
        return RtAudio::MACOSX_CORE;
    case Api::WASAPI:
        return RtAudio::WINDOWS_WASAPI;
    case Api::ASIO:
        return RtAudio::WINDOWS_ASIO;
    case Api::DS:
        return RtAudio::WINDOWS_DS;
    case Api::OSS:
        return RtAudio::LINUX_OSS;
    default:
        return RtAudio::UNSPECIFIED;
    }
}
} // namespace
#endif

#ifdef PIPEWIRE_BACKEND
#include "PipewireBackend.hpp"
#endif

namespace MayaFlux::Core {

std::unique_ptr<IAudioBackend> AudioBackendFactory::create_backend(
    Core::AudioBackendType type,
    std::optional<GlobalStreamInfo::AudioApi> api_preference)
{
    switch (type) {

#ifdef RTAUDIO_BACKEND
    case Core::AudioBackendType::RTAUDIO:
        if (api_preference) {
            auto pref_api = to_rtaudio_api(*api_preference);
            if (pref_api != RtAudio::UNSPECIFIED) {
                MF_INFO(Journal::Component::Core, Journal::Context::AudioBackend,
                    "Setting RtAudio preferred API to {}",
                    RtAudio::getApiDisplayName(pref_api));
                RtAudioSingleton::set_preferred_api(pref_api);
            }
        }
        return std::make_unique<RtAudioBackend>();
#endif

#ifdef PIPEWIRE_BACKEND
    case Core::AudioBackendType::PIPEWIRE:
        return std::make_unique<PipewireBackend>();
#endif

    default:
        throw std::runtime_error("Unsupported audio backend type");
    }
}

} // namespace MayaFlux::Core
