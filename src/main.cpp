#define MAYASIMPLE ;

#include "MayaFlux/MayaFlux.hpp"

void create()
{
    // All user code goes here

    // For examplle - Load audio file into memory and send to RtAudio sequentially
    auto container = vega.read("res/2.wav") | Audio;
}

int main()
{

#ifdef MAYASIMPLE
    register_container_context_operations();
    register_all_buffers();
    register_all_nodes();
#endif // MAYASIMPLE

    try {
        std::cout << "Initializing MayaFlux..." << std::endl;

        MayaFlux::Init();

        MayaFlux::Start();

        std::cout << "\n=== Audio Processing Active ===" << std::endl;

        create();

        std::cin.get();
        MayaFlux::End();
        std::cout << "✓ Clean shutdown" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
