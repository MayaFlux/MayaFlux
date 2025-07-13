#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/version.h"

#include "MayaFlux/Nodes/Generators/Impulse.hpp"
#include "MayaFlux/Nodes/Generators/Phasor.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Node/NodeBuffer.hpp"
#include "MayaFlux/Buffers/Recursive/FeedbackBuffer.hpp"

#include "MayaFlux/API/Proxy/Creator.hpp"

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
        MayaFlux::register_all_buffers();

        auto sine = create.sine(20.f, 100.f);
        auto sine1 = create.sine(200.f, 0.1f);
        auto sine2 = create.sine(500.f, 0.1f).domain(Audio).channel(0);
        // auto sine2 = create.domain(Audio).channel(1).sine(sine, sine1, 200.f, 0.1);

        // auto fb = bbcreate.domain(Audio).channel(0).feedback(0, 512, 0.9f);
        // for (auto& sample : fb->get_data()) {
        //     sample = MayaFlux::get_uniform_random();
        //     sample *= 0.1;
        // }

        auto impulse = create.impulse(1)[0] | Audio;
        impulse->on_impulse([](auto&) {
            std::cout << "Foo\n";
        });

        auto nn = create.node(0, 512, sine1, false)[1] | Audio;

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
