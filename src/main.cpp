#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Container/ContainerBuffer.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/Kakshya/Processors/ContiguousAccessProcessor.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/MayaFlux.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>
#include <numbers>

// REGISTER_NODE(MayaFlux::Nodes::Generator::Sine, sine);

std::atomic<bool> g_running { true };

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        std::cout << "\nShutting down..." << std::endl;
        g_running = false;
    }
}

std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> load_audio_file(const std::string& filepath)
{
    using namespace MayaFlux::IO;
    using namespace MayaFlux::Kakshya;

    // Create and initialize reader
    auto reader = std::make_unique<SoundFileReader>();
    reader->initialize_ffmpeg();

    if (!reader->can_read(filepath)) {
        std::cerr << "Cannot read file: " << filepath << std::endl;
        return nullptr;
    }

    // Configure reader
    reader->set_target_sample_rate(MayaFlux::get_sample_rate());
    reader->set_target_bit_depth(64);

    FileReadOptions options = FileReadOptions::EXTRACT_METADATA;
    if (!reader->open(filepath, options)) {
        std::cerr << "Failed to open file: " << reader->get_last_error() << std::endl;
        return nullptr;
    }

    // Create container and load data
    auto container = reader->create_container();
    auto sound_container = std::dynamic_pointer_cast<SoundFileContainer>(container);
    if (!sound_container) {
        std::cerr << "Failed to create sound container" << std::endl;
        return nullptr;
    }

    sound_container->set_memory_layout(MemoryLayout::ROW_MAJOR);

    if (!reader->load_into_container(sound_container)) {
        std::cerr << "Failed to load audio data: " << reader->get_last_error() << std::endl;
        return nullptr;
    }

    // Set up processor for streaming
    auto processor = std::make_shared<ContiguousAccessProcessor>();
    std::vector<u_int64_t> output_shape = { MayaFlux::get_buffer_size(), sound_container->get_num_channels() };
    processor->set_output_size(output_shape);
    processor->set_auto_advance(true);
    processor->set_active_dimensions({ 0 });

    sound_container->set_default_processor(processor);
    sound_container->mark_ready_for_processing(true);

    // Enable looping
    Region loop_region;
    loop_region.start_coordinates = { 5120 };
    // loop_region.end_coordinates = { sound_container->get_num_frames() - 1 };
    loop_region.end_coordinates = { sound_container->get_num_frames() / 32 };
    sound_container->set_loop_region(loop_region);
    sound_container->set_looping(true);

    std::cout << "✓ Loaded: " << filepath << std::endl;
    std::cout << "  Channels: " << sound_container->get_num_channels() << std::endl;
    std::cout << "  Frames: " << sound_container->get_num_frames() << std::endl;
    std::cout << "  Sample Rate: " << sound_container->get_sample_rate() << " Hz" << std::endl;

    return sound_container;
}

bool setup_audio_playback(std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container)
{
    auto buffer_manager = MayaFlux::get_buffer_manager();
    u_int32_t num_channels = container->get_num_channels();

    std::cout << "Setting up audio playback for " << num_channels << " channels..." << std::endl;

    for (u_int32_t channel = 0; channel < num_channels; ++channel) {
        // Create container buffer
        auto container_buffer = buffer_manager->create_buffer_for_token<MayaFlux::Buffers::ContainerBuffer>(
            MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND,
            channel,
            container,
            channel);

        container_buffer->initialize();

        std::cout << "✓ Created buffer for channel " << channel
                  << " (child buffers: " << buffer_manager->get_root_buffer(MayaFlux::Buffers::ProcessingToken::AUDIO_BACKEND, channel)->get_num_children() << ")" << std::endl;
    }

    return true;
}

int main()
{
    std::signal(SIGINT, signal_handler);

    try {
        // Initialize MayaFlux
        std::cout << "Initializing MayaFlux..." << std::endl;
        MayaFlux::Init();
        std::cout << "✓ MayaFlux initialized" << std::endl;
        std::cout << "  Sample rate: " << MayaFlux::get_sample_rate() << " Hz" << std::endl;
        std::cout << "  Buffer size: " << MayaFlux::get_buffer_size() << " samples" << std::endl;

        // Start audio engine
        MayaFlux::Start();
        std::cout << "✓ Audio engine started" << std::endl;

        // Try to load a test file
        std::vector<std::string> test_files = {
            "res/2.wav",
            "res/1.wav"
        };

        std::shared_ptr<MayaFlux::Kakshya::SoundFileContainer> container = nullptr;

        for (const auto& filepath : test_files) {
            if (std::filesystem::exists(filepath)) {
                container = load_audio_file(filepath);
                if (container)
                    break;
            }
        }

        if (!container) {
            std::cout << "No audio files found, creating synthetic sine wave..." << std::endl;

            // Create synthetic container
            container = std::make_shared<MayaFlux::Kakshya::SoundFileContainer>();

            constexpr u_int32_t sample_rate = 48000;
            constexpr u_int32_t channels = 2;
            constexpr u_int32_t duration_seconds = 5;
            constexpr u_int64_t total_frames = sample_rate * duration_seconds;

            container->setup(total_frames, sample_rate, channels);

            // Generate 440Hz sine wave
            std::vector<double> synthetic_data(total_frames * channels);
            const double frequency = 440.0;
            const double amplitude = 0.3;

            for (u_int64_t frame = 0; frame < total_frames; ++frame) {
                double sample = amplitude * std::sin(2.0 * std::numbers::pi * frequency * frame / sample_rate);
                synthetic_data[frame * channels] = sample; // Left
                synthetic_data[frame * channels + 1] = sample; // Right
            }

            container->set_raw_data(synthetic_data);
            container->set_memory_layout(MayaFlux::Kakshya::MemoryLayout::ROW_MAJOR);

            // Set up processor
            auto processor = std::make_shared<MayaFlux::Kakshya::ContiguousAccessProcessor>();
            std::vector<u_int64_t> output_shape = { MayaFlux::get_buffer_size(), channels };
            processor->set_output_size(output_shape);
            processor->set_auto_advance(true);
            processor->set_active_dimensions({ 0 });

            container->set_default_processor(processor);
            container->mark_ready_for_processing(true);

            // Enable looping
            MayaFlux::Kakshya::Region loop_region;
            loop_region.start_coordinates = { 0 };
            loop_region.end_coordinates = { total_frames / 128 };
            container->set_loop_region(loop_region);
            container->set_looping(true);

            std::cout << "✓ Created synthetic 440Hz sine wave" << std::endl;
        }

        // Set up audio playback
        if (!setup_audio_playback(container)) {
            std::cerr << "Failed to set up audio playback" << std::endl;
            return 1;
        }

        std::cout << "\n=== Audio Processing Active ===" << std::endl;
        std::cout << "Playing audio... Press Ctrl+C to stop" << std::endl;

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "Stopping audio engine..." << std::endl;
        MayaFlux::End();
        std::cout << "✓ Clean shutdown" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
