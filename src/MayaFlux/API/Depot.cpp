#include "Depot.hpp"
#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

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
        MF_ERROR(Journal::Component::API, Journal::Context::FileIO, "Failed to open file: {}", reader->get_last_error());
        return nullptr;
    }

    auto container = reader->create_container();
    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sound_container) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime, "Failed to create sound container");
        return nullptr;
    }

    sound_container->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    if (!reader->load_into_container(sound_container)) {
        MF_ERROR(Journal::Component::API, Journal::Context::Runtime, "Failed to load audio data: {}", reader->get_last_error());
        return nullptr;
    }

    auto existing_processor = std::dynamic_pointer_cast<Kakshya::ContiguousAccessProcessor>(
        sound_container->get_default_processor());

    if (existing_processor) {
        std::vector<uint64_t> output_shape = { MayaFlux::Config::get_buffer_size(), sound_container->get_num_channels() };
        existing_processor->set_output_size(output_shape);
        existing_processor->set_auto_advance(true);

        MF_DEBUG(Journal::Component::API, Journal::Context::ContainerProcessing, "Configured existing ContiguousAccessProcessor");
    } else {
        MF_TRACE(Journal::Component::API, Journal::Context::ContainerProcessing, "No default processor found, creating a new ContiguousAccessProcessor");

        auto processor = std::make_shared<Kakshya::ContiguousAccessProcessor>();
        std::vector<uint64_t> output_shape = { MayaFlux::Config::get_buffer_size(), sound_container->get_num_channels() };
        processor->set_output_size(output_shape);
        processor->set_auto_advance(true);

        sound_container->set_default_processor(processor);
    }

    MF_INFO(
        Journal::Component::API,
        Journal::Context::FileIO,
        "Loaded audio file: {} | Channels: {} | Frames: {} | Sample Rate: {} Hz",
        filepath,
        sound_container->get_num_channels(),
        sound_container->get_num_frames(),
        sound_container->get_sample_rate());

    return sound_container;
}

std::vector<std::shared_ptr<Buffers::ContainerBuffer>> hook_sound_container_to_buffers(const std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer>& container)
{
    auto buffer_manager = MayaFlux::get_buffer_manager();
    uint32_t num_channels = container->get_num_channels();
    std::vector<std::shared_ptr<Buffers::ContainerBuffer>> created_buffers;

    MF_TRACE(
        Journal::Component::API,
        Journal::Context::BufferManagement,
        "Setting up audio playback for {} channels...",
        num_channels);

    for (uint32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = buffer_manager->create_audio_buffer<MayaFlux::Buffers::ContainerBuffer>(
            MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();

        created_buffers.push_back(std::move(container_buffer));

        MF_INFO(
            Journal::Component::API,
            Journal::Context::BufferManagement,
            "✓ Created buffer for channel {}",
            channel);
    }

    return created_buffers;
}

}
