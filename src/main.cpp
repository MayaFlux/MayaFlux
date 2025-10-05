#include "MayaFlux/Core/Engine.hpp"
#define MAYASIMPLE ;

#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/version.h"

#ifdef __has_include
#if __has_include("user_project.hpp")
#include "user_project.hpp"
#define HAS_USER_PROJECT
#endif
#endif

void create()
{
#ifdef HAS_USER_PROJECT
    compose();
#else
    auto container = vega.read("res/audio.wav") | Audio;
#endif
}

int main()
{

#ifdef MAYASIMPLE
    register_container_context_operations();
    register_all_buffers();
    register_all_nodes();
#endif // MAYASIMPLE

    try {
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "=== MayaFlux Audio Engine ===");
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "Version: {}", "0.1.0");
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "");

        MayaFlux::Init();

        MayaFlux::Start();

        MF_PRINT(Journal::Component::USER, Journal::Context::Init, "=== Audio Processing Active ===");

        create();

        std::cout << "Press any key to stop..." << std::endl;
        std::cin.get();
        MayaFlux::End();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
