#include "Depot.hpp"
#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"

namespace MayaFlux {

std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath)
{
    auto reader = std::make_unique<IO::SoundFileReader>();
    MayaFlux::IO::SoundFileReader::initialize_ffmpeg();

    if (!reader->can_read(filepath)) {
        std::cerr << "Cannot read file: " << filepath << '\n';
        return nullptr;
    }

    reader->set_target_sample_rate(MayaFlux::Config::get_sample_rate());
    reader->set_target_bit_depth(64);
    reader->set_audio_options(IO::AudioReadOptions::DEINTERLEAVE);

    IO::FileReadOptions options = IO::FileReadOptions::EXTRACT_METADATA;
    if (!reader->open(filepath, options)) {
        std::cerr << "Failed to open file: " << reader->get_last_error() << '\n';
        return nullptr;
    }

    auto container = reader->create_container();
    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sound_container) {
        std::cerr << "Failed to create sound container" << '\n';
        return nullptr;
    }

    sound_container->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    if (!reader->load_into_container(sound_container)) {
        std::cerr << "Failed to load audio data: " << reader->get_last_error() << '\n';
        return nullptr;
    }

    auto existing_processor = std::dynamic_pointer_cast<Kakshya::ContiguousAccessProcessor>(
        sound_container->get_default_processor());

    if (existing_processor) {
        std::vector<uint64_t> output_shape = { MayaFlux::Config::get_buffer_size(), sound_container->get_num_channels() };
        existing_processor->set_output_size(output_shape);
        existing_processor->set_auto_advance(true);

        std::cout << "✓ Configured existing ContiguousAccessProcessor" << '\n';
    } else {
        std::cerr << "Warning: No default processor found, creating a new one" << '\n';

        auto processor = std::make_shared<Kakshya::ContiguousAccessProcessor>();
        std::vector<uint64_t> output_shape = { MayaFlux::Config::get_buffer_size(), sound_container->get_num_channels() };
        processor->set_output_size(output_shape);
        processor->set_auto_advance(true);

        sound_container->set_default_processor(processor);
    }

    std::cout << "✓ Loaded: " << filepath << '\n';
    std::cout << "  Channels: " << sound_container->get_num_channels() << '\n';
    std::cout << "  Frames: " << sound_container->get_num_frames() << '\n';
    std::cout << "  Sample Rate: " << sound_container->get_sample_rate() << " Hz" << '\n';

    return sound_container;
}

void hook_sound_container_to_buffers(std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container)
{
    auto buffer_manager = MayaFlux::get_buffer_manager();
    uint32_t num_channels = container->get_num_channels();

    std::cout << "Setting up audio playback for " << num_channels << " channels..." << '\n';

    for (uint32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = buffer_manager->create_audio_buffer<MayaFlux::Buffers::ContainerBuffer>(
            MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();

        std::cout << "✓ Created buffer for channel " << channel << '\n';
    }
}

}
