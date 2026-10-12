#include "../test_config.h"

#include "MayaFlux/Nodes/Generators/Polynomial.hpp"
#include "MayaFlux/Nodes/Network/ResonatorNetwork.hpp"

#include <chrono>
#include <iostream>

namespace MayaFlux::Test {

using Nodes::Generator::Polynomial;
using Nodes::Network::ResonatorNetwork;

namespace {

    constexpr unsigned int batch_size = 512;
    constexpr size_t warm_batches = 8;
    constexpr size_t timed_batches = 200;

    std::shared_ptr<Nodes::Node> make_noise()
    {
        return std::make_shared<Polynomial>([state = uint64_t { 1 }](double) mutable {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            return static_cast<double>(state >> 11) / 9007199254740992.0 * 2.0 - 1.0;
        });
    }

    double load_for(size_t resonators, bool control_rate_writes)
    {
        std::vector<double> frequencies(resonators);
        std::vector<double> qs(resonators, 200.0);
        std::vector<double> decays(resonators, 0.5);
        std::vector<double> gains(resonators, 0.5);
        for (size_t i = 0; i < resonators; ++i) {
            frequencies.at(i) = 100.0 + 31.0 * static_cast<double>(i % 300);
        }

        ResonatorNetwork net(frequencies, qs);
        net.set_exciter(make_noise());

        for (size_t b = 0; b < warm_batches; ++b) {
            net.process_batch(batch_size);
        }

        const auto start = std::chrono::steady_clock::now();
        for (size_t b = 0; b < timed_batches; ++b) {
            if (control_rate_writes) {
                net.retune(frequencies);
                net.set_decay(decays);
                net.set_resonator_gain(gains);
            }
            net.process_batch(batch_size);
        }
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

        const double audio_seconds = static_cast<double>(timed_batches * batch_size) / net.get_sample_rate();
        return elapsed.count() / audio_seconds;
    }

}

TEST(ResonatorBenchTest, LoadAgainstResonatorCount)
{
    std::cout << "resonators  load(plain)  load(with retune+decay+gain every buffer)\n";
    for (const size_t count : { 50UL, 100UL, 300UL, 600UL }) {
        const double plain = load_for(count, false);
        const double written = load_for(count, true);
        std::cout << count << "  " << plain * 100.0 << "%  " << written * 100.0 << "%\n";

        EXPECT_GT(plain, 0.0);
        EXPECT_GT(written, 0.0);
    }
}

}
