#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Containers/DataProcessor.hpp"
#include "MayaFlux/Containers/Source/SoundFileContainer.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/MayaFlux.hpp"

using namespace MayaFlux;
using namespace MayaFlux::IO;
using namespace MayaFlux::Containers;

/**
 * A simple program that demonstrates loading and playing an audio file
 * using the SoundFileReader and SoundFileContainer classes.
 *
 * This example uses the interleaved data approach where the data remains
 * interleaved in the container and the ContiguousAccessProcessor handles
 * deinterleaving as needed.
 */
int main(int argc, char** argv)
{
    // if (argc < 2) {
    //     std::cerr << "Usage: " << argv[0] << " <audio_file>" << std::endl;
    //     return 1;
    // }

    std::string file_path = "res/1.wav";

    try {
        // Initialize the MayaFlux engine
        std::cout << "Initializing MayaFlux engine..." << std::endl;
        Init();

        SoundFileReader reader;
        auto container = std::make_shared<SoundFileContainer>();

        // Load the audio file into the container
        std::cout << "Loading audio file: " << file_path << std::endl;
        if (!reader.read_file_to_container(file_path, std::static_pointer_cast<Containers::SignalSourceContainer>(container))) {
            std::cerr << "Error loading file: " << reader.get_last_error() << std::endl;
            return 1;
        }

        // Display file information
        std::cout << "File loaded successfully!" << std::endl;
        std::cout << std::format("  Sample rate: {} Hz", container->get_sample_rate()) << std::endl;
        std::cout << std::format("  Channels: {}", container->get_num_audio_channels()) << std::endl;
        std::cout << std::format("  Total frames: {}", container->get_num_frames_total()) << std::endl;
        std::cout << std::format("  Duration: {:.2f} seconds", container->get_duration_seconds()) << std::endl;

        // Enable looping
        container->set_looping(true);
        std::cout << "Looping enabled" << std::endl;

        // Get all audio buffers from the container
        auto buffers = container->get_all_buffers();
        std::cout << std::format("Container has {} channel buffers", buffers.size()) << std::endl;

        // Connect the buffers to audio output channels
        for (size_t i = 0; i < buffers.size(); ++i) {
            MayaFlux::get_buffer_manager()->add_buffer_to_channel(static_cast<unsigned int>(i), buffers[i]);
            std::cout << std::format("Connected buffer {} to output channel {}", i, i) << std::endl;
        }

        // Start playback
        std::cout << "Starting playback..." << std::endl;
        Start();

        double process_interval = static_cast<double>(get_buffer_size()) / get_sample_rate();

        /* schedule_task("BufferPlayer", schedule_metro(process_interval, [container]() {
            if (container->get_processing_state() == ProcessingState::READY) {
                container->process_default();
            } else if (container->get_processing_state() == ProcessingState::IDLE) {
                container->update_processing_state(ProcessingState::READY);
            }
        }),
            true); */

        get_context().register_process_hook("BufferPlayer", [&container](u_int32_t num_frames) {
            std::cout << "Calling attached process\n";
            container->process_default(); }, Core::HookPosition::PRE_PROCESS);

        // Wait for user to press Enter
        std::cout
            << "Press Enter to stop playback..." << std::endl;
        std::cin.get();

        // Stop playback and clean up
        std::cout << "Stopping playback..." << std::endl;
        End();

        std::cout << "Playback complete" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
