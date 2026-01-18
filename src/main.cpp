#ifdef __has_include
#if __has_include("user_project.hpp")
#include "user_project.hpp"
#define HAS_USER_PROJECT
#else
#define MAYASIMPLE
#include "MayaFlux/MayaFlux.hpp"
#endif
#endif

void initialize()
{
#ifdef HAS_USER_PROJECT
    try {
        settings();
    } catch (const std::exception& e) {
        MF_ERROR(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "Error during user initialization: {}", e.what());
    }
#endif
}

void run()
{
#ifdef HAS_USER_PROJECT
    try {
        compose();
    } catch (const std::exception& e) {
        MF_ERROR(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Runtime, "Error running user code: {}", e.what());
    }
#endif
}

int main()
{
    try {
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "=== MayaFlux Creative Coding Framework ===");
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "Version: {}", "0.2.0");
        MF_PRINT(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "");

        initialize();

        MayaFlux::Init();

        MayaFlux::Start();

        MF_PRINT(Journal::Component::USER, Journal::Context::Init, "=== Audio Processing Active ===");

        run();

        std::cout << "Press Enter [Return] to stop...\n";
        MayaFlux::Await();

        MayaFlux::End();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::flush;
        return 1;
    }

    return 0;
}
