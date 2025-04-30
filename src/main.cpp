#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/IO/SoundFileReader.hpp"
#include "MayaFlux/Kakshya/DataProcessor.hpp"
#include "MayaFlux/Kakshya/Source/SoundFileContainer.hpp"
#include "MayaFlux/MayaFlux.hpp"

using namespace MayaFlux;
using namespace MayaFlux::IO;
using namespace MayaFlux::Kakshya;

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
        std::cout << "Initializing MayaFlux engine..." << std::endl;
        Init();

        SoundFileReader reader;
        auto container = std::make_shared<SoundFileContainer>();

        std::cout << "Loading audio file: " << file_path << std::endl;
        if (!reader.read_file_to_container(file_path, std::static_pointer_cast<Kakshya::SignalSourceContainer>(container))) {
            std::cerr << "Error loading file: " << reader.get_last_error() << std::endl;
            return 1;
        }

        std::cout << "File loaded successfully!" << std::endl;
        std::cout << std::format("  Sample rate: {} Hz", container->get_sample_rate()) << std::endl;
        std::cout << std::format("  Channels: {}", container->get_num_audio_channels()) << std::endl;
        std::cout << std::format("  Total frames: {}", container->get_num_frames_total()) << std::endl;
        std::cout << std::format("  Duration: {:.2f} seconds", container->get_duration_seconds()) << std::endl;

        container->set_looping(true);
        std::cout << "Looping enabled" << std::endl;

        auto buffers = container->get_all_buffers();
        std::cout << std::format("Container has {} channel buffers", buffers.size()) << std::endl;

        for (size_t i = 0; i < buffers.size(); ++i) {
            MayaFlux::get_buffer_manager()->add_buffer_to_channel(static_cast<unsigned int>(i), buffers[i]);
            std::cout << std::format("Connected buffer {} to output channel {}", i, i) << std::endl;
        }

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

        get_context().register_process_hook("BufferPlayer", [&container](u_int32_t num_frames)
            //
            { container->process_default(); },
            Core::HookPosition::PRE_PROCESS);

        std::cout
            << "Press Enter to stop playback..." << std::endl;
        std::cin.get();

        std::cout << "Stopping playback..." << std::endl;
        End();

        std::cout << "Playback complete" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
