#include "Lila/Lila.hpp"

#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"

#include <iostream>

int main()
{
    std::cout << "Initializing MayaFlux...\n";
    MayaFlux::Init(48000, 512, 2, 2);

    std::cout << "Initializing Lila...\n";
    Lila::Lila lila;
    if (!lila.initialize()) {
        std::cerr << "Failed to initialize Lila\n";
        return 1;
    }

    std::cout << "\n=== Test 1: Access MayaFlux API ===\n";
    const char* test1 = R"(
        #define MAYASIMPLE
        #include "MayaFlux/MayaFlux.hpp"
        #include "MayaFlux/Nodes/Generators/Sine.hpp"
        #include "MayaFlux/API/Proxy/Creator.hpp"
        #include "MayaFlux/Core/GlobalGraphicsInfo.hpp"
        #include "MayaFlux/Core/Windowing/WindowManager.hpp"
        #include "MayaFlux/Core/Backends/Windowing/Glfw/GlfwWindow.hpp"

        using namespace MayaFlux;


        MayaFlux::register_container_context_operations();
        MayaFlux::register_all_buffers();
        MayaFlux::register_all_nodes();
        MayaFlux::Init();
        
        std::cout << "Sample rate: " << MayaFlux::Config::get_sample_rate() << "\n";
        std::cout << "Buffer size: " << MayaFlux::Config::get_buffer_size() << "\n";
        std::cout << "Output channels: " << MayaFlux::Config::get_num_out_channels() << "\n";
        MayaFlux::Start();

        // auto node = vega.sine(440.f, 0.1f)[0] | Audio;
        // MayaFlux::register_audio_node(node, 0);
        // auto node = MayaFlux::create_node<MayaFlux::Nodes::Generator::Sine>();
        // for (int i = 0; i < 10; ++i) {
        //     std::cout << "Sine output [" << i << "]: " << node->process_sample(0.) << "\n";
        // }
        // MayaFlux::schedule_metro(1.0, []() {
        //     std::cout << "Metro tick!\n";
        // }); 

        auto main_window = MayaFlux::create_window({ .title = "Main Output",
            .width = 1920,
            .height = 1080 });

        main_window->show();

        std::cin.get();

        MayaFlux::End();
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

    MayaFlux::End();
    std::cout << "All tests complete!\n";

    return 0;
}
