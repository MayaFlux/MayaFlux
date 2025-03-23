#include "MayaFlux/Core/Engine.hpp"
#include "MayaFlux/Nodes/Generators/Sine.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

int main()
{
    MayaFlux::Core::Engine engine;
    MayaFlux::Core::GlobalStreamInfo stream_info;
    stream_info.buffer_size = 512;
    stream_info.sample_rate = 48000;
    stream_info.num_channels = 2;

    engine.Init(stream_info);

    using namespace MayaFlux::Nodes::Generator;

    std::shared_ptr<Sine> sine = std::make_shared<Sine>((std::make_shared<Sine>(1, 5, 0.0, false) + std::make_shared<Sine>(50, 100, 0.0, false)), 440, 0.5, 0.0);

    engine.add_processor([](double* in, double* out, unsigned int nFrames) {
        const double masterVolume = 0.8;
        unsigned int numSamples = nFrames * 2;

        for (unsigned int i = 0; i < numSamples; i++) {
            double current_sample = out[i];

            current_sample *= masterVolume;
            if (current_sample > 1.f) {
                current_sample = 1.f;
            } else if (current_sample < -1.f) {
                current_sample = -1.f;
            }
            out[i] = current_sample;
        }
    });

    std::cout << "Starting audio engine..." << std::endl;
    engine.Start();

    std::cout << "Audio running. Press Enter to stop." << std::endl;
    std::cin.get();
    return 0;
}
