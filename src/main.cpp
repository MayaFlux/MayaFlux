#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/version.h"
#include <csignal>

std::atomic<bool> g_running { true };

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        std::cout << "\nShutting down MayaFlux..." << std::endl;
        g_running = false;
    }
}

int main()
{
    std::cout << "MayaFlux Audio Engine v" << MAYAFLUX_VERSION << std::endl;

    std::signal(SIGINT, signal_handler);

    try {
        MayaFlux::Init();

        MayaFlux::Start();

        std::cout << "Audio engine running. Press Ctrl+C to stop." << std::endl;
        std::cout << "Sample rate: " << MayaFlux::get_sample_rate() << " Hz" << std::endl;
        std::cout << "Buffer size: " << MayaFlux::get_buffer_size() << " samples" << std::endl;

        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        MayaFlux::End();
        std::cout << "MayaFlux shut down successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
