#include "Lila/Lila.hpp"
#include <iostream>

int main()
{
    Lila::Lila lila;

    if (!lila.initialize()) {
        std::cerr << "Failed to initialize: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== TEST 1: Create a Sine generator ==========\n";
    bool success = lila.eval(R"(
        MayaFlux::Init();
        // std::cout << MayaFlux::Config::get_sample_rate() << " Hz sample rate\n";
        // auto node = std::make_shared<Sine>(440.0f, 0.1);
        // std::cout << "Created sine generator at 440Hz\n";
        MayaFlux::Start();
        // auto node = vega.sine(440.0f, 0.1f)[0] | Audio;
        std::shared_ptr<Sine> node = vega.sine(440.0f, 0.1f)[0] | Audio;
        MayaFlux::schedule_metro(1, []() {
            std::cout << "Metro tick!\n";
        });
        // node >> Kriya::DAC::instance();
        // std::shared_ptr<Kakshya::SoundFileContainer> container = vega.read("res/audio.wav") | Audio;
        // auto container = vega.read("res/audio.wav") | Audio;
    )");

    if (!success) {
        std::cerr << "Eval 1 failed: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== TEST 2: Modify the sine frequency ==========\n";
    success = lila.eval(R"(
        node->set_frequency(880.0f);
        std::cout << "Changed sine to 880Hz\n";
        // std::shared_ptr<MayaFlux::Core::Window> main_window = MayaFlux::create_window({ .title = "Main Output",
        //     .width = 1920,
        //     .height = 1080 });
        // main_window->show();
        // std::shared_ptr<Phasor> phasor = std::make_shared<Phasor>(10, 100.0f);
        std::shared_ptr<Phasor> phasor = std::make_shared<Phasor>(10000, 0.7f);
        phasor * node;
        // node->set_frequency_modulator(phasor);
    )");

    if (!success) {
        std::cerr << "Eval 2 failed: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== TEST 3: Create another generator ==========\n";
    success = lila.eval(R"(
        std::cout << "Created phasor at 220Hz\n";
        std::cout << "Sine and phasor both exist!\n";
        MayaFlux::End();
    )");

    if (!success) {
        std::cerr << "Eval 3 failed: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== ALL MAYAFLUX TESTS PASSED ==========\n";
    return 0;
}
