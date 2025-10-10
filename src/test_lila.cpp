/* #include "Lila/Lila.hpp"

#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

#include <iostream>

int main()
{
    std::cout << "Initializing Lila...\n";
    Lila::Lila lila;
    if (!lila.initialize()) {
        std::cerr << "Failed to initialize Lila\n";
        return 1;
    }

    std::cout << "\n=== Test 1: Access MayaFlux API ===\n";
    const char* test1 = R"(
        #include "MayaFlux/MayaFlux.hpp"

        MayaFlux::Init();

        std::cout << "Sample rate: " << MayaFlux::Config::get_sample_rate() << "\n";
        std::cout << "Buffer size: " << MayaFlux::Config::get_buffer_size() << "\n";
        std::cout << "Output channels: " << MayaFlux::Config::get_num_out_channels() << "\n";

        MayaFlux::Start();

        //auto container = vega.read("res/audio.wav") | Audio;

         auto node = vega.sine(440.f, 0.1f)[0] | Audio;

        // MayaFlux::schedule_metro(1.0, []() {
        //     std::cout << "Metro tick!\n";
        // });

        // auto main_window = MayaFlux::create_window({ .title = "Main Output",
        //     .width = 1920,
        //     .height = 1080 });

        // main_window->show();

        std::cin.get();

        //MayaFlux::End();
    )";

    if (lila.eval(test1)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error: " << lila.get_last_error() << "\n\n";
    }

    std::cout << "=== Test 2: C++20 Features ===\n";
    const char* test2 = R"(
        auto ptr = std::make_shared<std::vector<float>>(512);
        std::cout << "Created vector with " << ptr->size() << " elements\n";

        for (size_t i = 0; i < 10; ++i) {
            (*ptr)[i] = i * 0.1f;
        }
        std::cout << "First 10 values initialized\n";
    )";

    if (lila.eval(test2)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error: " << lila.get_last_error() << "\n\n";
    }

    std::cout << "=== Test 3: Multiple Evals (no conflicts) ===\n";
    const char* test3a = R"(
        std::cout << "First eval\n";
    )";

    const char* test3b = R"(
        std::cout << "Second eval\n";
    )";

    if (lila.eval(test3a) && lila.eval(test3b)) {
        std::cout << "✓ Success\n\n";
    } else {
        std::cerr << "✗ Error\n\n";
    }

    return 0;
} */

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
        std::cout << MayaFlux::Config::get_sample_rate() << " Hz sample rate\n";
        // std::shared_ptr<Sine> sine = std::make_shared<Sine>(440.0f, 0.1);
        std::cout << "Created sine generator at 440Hz\n";
        MayaFlux::Start();
        std::shared_ptr<Sine> node = vega.sine(440.0f, 0.1f)[0] | Audio;
        MayaFlux::schedule_metro(0.3, []() {
            std::cout << "Metro tick!\n";
        });
    )");

    if (!success) {
        std::cerr << "Eval 1 failed: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== TEST 2: Modify the sine frequency ==========\n";
    success = lila.eval(R"(
        // MayaFlux::register_audio_node(sine);
        node->set_frequency(880.0f);
        std::cout << "Changed sine to 880Hz\n";
        std::shared_ptr<MayaFlux::Core::Window> main_window = MayaFlux::create_window({ .title = "Main Output",
            .width = 1920,
            .height = 1080 });
        main_window->show();
    )");

    if (!success) {
        std::cerr << "Eval 2 failed: " << lila.get_last_error() << "\n";
        return 1;
    }

    std::cout << "\n========== TEST 3: Create another generator ==========\n";
    success = lila.eval(R"(
        std::shared_ptr<Phasor> phasor = std::make_shared<Phasor>(220.0f);
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
