#include "AudioBackend.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

#ifdef PIPEWIRE_BACKEND
#include "PipewireBackend.hpp"
#endif

#ifdef WASAPI_BACKEND
#include "WasapiBackend.hpp"
#endif

#ifdef COREAUDIO_BACKEND
#include "CoreAudioBackend.hpp"
#endif

namespace MayaFlux::Core {

std::unique_ptr<IAudioBackend> AudioBackendFactory::create_backend(Core::AudioBackendType type)
{
    switch (type) {

#ifdef PIPEWIRE_BACKEND
    case Core::AudioBackendType::PIPEWIRE:
        return std::make_unique<PipewireBackend>();
#endif

#ifdef WASAPI_BACKEND
    case Core::AudioBackendType::WASAPI:
        return std::make_unique<WasapiBackend>();
#endif

#ifdef COREAUDIO_BACKEND
    case Core::AudioBackendType::COREAUDIO:
        return std::make_unique<CoreAudioBackend>();
#endif

    default:
        error<std::runtime_error>(Journal::Component::Core, Journal::Context::AudioBackend, std::source_location::current(),
            "Unsupported audio backend type: {}", static_cast<int>(type));
    }
}

} // namespace MayaFlux::Core
