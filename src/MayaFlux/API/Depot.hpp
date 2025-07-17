#pragma once

namespace MayaFlux {

namespace IO {
    class SoundFileReader;
}

namespace Kakshya {
    class SoundFileContainer;
}

std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath);

void hook_sound_container_to_buffers(std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container);

void register_container_context_operations();

}
