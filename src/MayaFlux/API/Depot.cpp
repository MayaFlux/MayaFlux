#include "Depot.hpp"
#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"
#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"

#include "MayaFlux/API/Proxy/Creator.hpp"

namespace MayaFlux {

std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath)
{
    auto reader = std::make_unique<IO::SoundFileReader>();
    reader->initialize_ffmpeg();

    if (!reader->can_read(filepath)) {
        std::cerr << "Cannot read file: " << filepath << std::endl;
        return nullptr;
    }

    reader->set_target_sample_rate(MayaFlux::get_sample_rate());
    reader->set_target_bit_depth(64);

    IO::FileReadOptions options = IO::FileReadOptions::EXTRACT_METADATA;
    if (!reader->open(filepath, options)) {
        std::cerr << "Failed to open file: " << reader->get_last_error() << std::endl;
        return nullptr;
    }

    auto container = reader->create_container();
    auto sound_container = std::dynamic_pointer_cast<Kakshya::SoundFileContainer>(container);
    if (!sound_container) {
        std::cerr << "Failed to create sound container" << std::endl;
        return nullptr;
    }

    sound_container->set_memory_layout(Kakshya::MemoryLayout::ROW_MAJOR);

    if (!reader->load_into_container(sound_container)) {
        std::cerr << "Failed to load audio data: " << reader->get_last_error() << std::endl;
        return nullptr;
    }

    auto existing_processor = std::dynamic_pointer_cast<Kakshya::ContiguousAccessProcessor>(
        sound_container->get_default_processor());

    if (existing_processor) {
        std::vector<u_int64_t> output_shape = { MayaFlux::get_buffer_size(), sound_container->get_num_channels() };
        existing_processor->set_output_size(output_shape);
        existing_processor->set_auto_advance(true);
        existing_processor->set_active_dimensions({ 0 });

        std::cout << "✓ Configured existing ContiguousAccessProcessor" << std::endl;
    } else {
        std::cerr << "Warning: No default processor found, creating a new one" << std::endl;

        auto processor = std::make_shared<Kakshya::ContiguousAccessProcessor>();
        std::vector<u_int64_t> output_shape = { MayaFlux::get_buffer_size(), sound_container->get_num_channels() };
        processor->set_output_size(output_shape);
        processor->set_auto_advance(true);
        processor->set_active_dimensions({ 0 });

        sound_container->set_default_processor(processor);
    }

    std::cout << "✓ Loaded: " << filepath << std::endl;
    std::cout << "  Channels: " << sound_container->get_num_channels() << std::endl;
    std::cout << "  Frames: " << sound_container->get_num_frames() << std::endl;
    std::cout << "  Sample Rate: " << sound_container->get_sample_rate() << " Hz" << std::endl;

    return sound_container;
}

void hook_sound_container_to_buffers(std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container)
{
    auto buffer_manager = MayaFlux::get_buffer_manager();
    u_int32_t num_channels = container->get_num_channels();

    std::cout << "Setting up audio playback for " << num_channels << " channels..." << std::endl;

    for (u_int32_t channel = 0; channel < num_channels; ++channel) {
        auto container_buffer = buffer_manager->create_buffer_for_token<MayaFlux::Buffers::ContainerBuffer>(
            MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();

        std::cout << "✓ Created buffer for channel " << channel << std::endl;
    }
}

void hook_sound_container_to_buffers_with_context(
    std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container,
    const CreationContext& context)
{
    auto buffer_manager = MayaFlux::get_buffer_manager();
    u_int32_t num_channels = container->get_num_channels();

    Domain target_domain = context.domain.value_or(Domain::AUDIO);
    MayaFlux::Buffers::ProcessingToken token = (target_domain == Domain::AUDIO)
        ? MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND
        : MayaFlux::Buffers::ProcessingToken::GRAPHICS_BACKEND;

    if (context.channel.has_value()) {
        u_int32_t target_channel = context.channel.value();
        if (target_channel < num_channels) {
            auto container_buffer = buffer_manager->create_buffer_for_token<MayaFlux::Buffers::ContainerBuffer>(
                token, target_channel, container, target_channel);
            container_buffer->initialize();
            std::cout << "✓ Created buffer for channel " << target_channel << std::endl;
        } else {
            std::cerr << "Requested channel " << target_channel << " exceeds available channels (" << num_channels << ")" << std::endl;
        }
    } else {
        for (u_int32_t channel = 0; channel < num_channels; ++channel) {
            auto container_buffer = buffer_manager->create_buffer_for_token<MayaFlux::Buffers::ContainerBuffer>(
                token, channel, container, channel);
            container_buffer->initialize();
            std::cout << "✓ Created buffer for channel " << channel << std::endl;
        }
    }
}

void register_container_context_operations()
{
    ContextAppliers::set_container_loader([](const std::string& filepath) -> std::shared_ptr<Kakshya::SoundFileContainer> {
        return load_audio_file(filepath);
    });

    ContextAppliers::set_container_context_applier([](std::shared_ptr<Kakshya::SoundFileContainer> container, const CreationContext& context) {
        if (!container) {
            std::cerr << "Cannot apply context to null container" << std::endl;
            return;
        }

        if (context.domain.has_value()) {
            hook_sound_container_to_buffers_with_context(container, context);
        } else {
            std::cout << "Container loaded but not hooked to buffers (no domain specified)" << std::endl;
        }
    });
}

}
