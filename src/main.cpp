#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/Generators/Impulse.hpp"
#include "MayaFlux/Nodes/Generators/Phasor.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/version.h"

#include "MayaFlux/Nodes/NodeGraphManager.hpp"

#include "MayaFlux/API/Proxy/NodeProxy.hpp"

#include <csignal>

std::atomic<bool> g_running { true };

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        std::cout << "\nShutting down MayaFlux..." << std::endl;
        g_running = false;
    }
}

using namespace MayaFlux;

int main()
{
    std::cout << "MayaFlux Audio Engine v" << MAYAFLUX_VERSION << std::endl;

    std::signal(SIGINT, signal_handler);

    try {
        MayaFlux::Init();

        MayaFlux::Start();

        MayaFlux::register_all_nodes();

        auto sine = create.sine(20.f, 100.f);
        auto sine1 = create.sine(20.f, 10.f);
        // auto sine2 = create.sine(200).domain(Audio).channel(0);
        // auto sine2 = create.domain(Audio).channel(1).sine(sine, sine1, 200.f, 0.1);
        auto phasor = create.phasor(5);
        sine1* sine* phasor;

        auto impulse = create.impulse(1)[0] | Audio;
        // auto impulse = create.domain(MayaFlux::Audio).channel(0).impulse(1);
        impulse->on_impulse([](auto&) {
            std::cout << "Foo\n";
        });

        MayaFlux::attach_quick_process_to_audio_channel(
            [](std::shared_ptr<MayaFlux::Buffers::AudioBuffer> buffer) {
                auto& data = buffer->get_data();
                for (double& sample : data) {
                    sample *= MayaFlux::get_uniform_random();
                    sample *= 0.1;
                }
            },
            0);

        std::cout << "Audio engine running. Press Ctrl+C to stop." << std::endl;
        std::cout << "Sample rate: " << MayaFlux::get_sample_rate() << " Hz" << std::endl;
        std::cout << "Buffer size: " << MayaFlux::get_buffer_size() << " samples" << std::endl;

        std::cin.get();

        MayaFlux::End();
        std::cout << "MayaFlux shut down successfully." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
