#include "RtAudioSingleton.hpp"

namespace MayaFlux::Core {
std::unique_ptr<RtAudio> RtAudioSingleton::s_instance = nullptr;
std::mutex RtAudioSingleton::s_mutex;
bool RtAudioSingleton::s_stream_open = false;
std::optional<RtAudio::Api> RtAudioSingleton::s_preferred_api = std::nullopt;
}
