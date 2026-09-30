#pragma once

#include "MayaFlux/Kakshya/NDData/NDData.hpp"

namespace MayaFlux::Buffers {
class AudioBuffer;
}

namespace MayaFlux::Kakshya {
class DynamicSoundStream;
}

namespace MayaFlux::Kriya::detail {

Kakshya::DataVariant extract_buffer_data(const std::shared_ptr<Buffers::AudioBuffer>& buffer, bool should_process = false);
void write_to_buffer(const std::shared_ptr<Buffers::AudioBuffer>& buffer, const Kakshya::DataVariant& data);
void write_to_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, const Kakshya::DataVariant& data, uint32_t channel);
Kakshya::DataVariant read_from_container(const std::shared_ptr<Kakshya::DynamicSoundStream>& container, uint64_t start, uint32_t length);

}
