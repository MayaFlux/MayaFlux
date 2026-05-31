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

int main(int argc, char* argv[])
{
    try {
        MF_LOG(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "=== MayaFlux Creative Coding Framework ===");
        MF_LOG(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init,
            "Version: {}.{}.{}", MAYAFLUX_VERSION_MAJOR, MAYAFLUX_VERSION_MINOR, MAYAFLUX_VERSION_PATCH);
        MF_LOG(MayaFlux::Journal::Component::USER, MayaFlux::Journal::Context::Init, "");

        std::string config_path;
        for (int i = 1; i < argc - 1; ++i) {
            if (std::string_view(argv[i]) == "--config") {
                config_path = argv[i + 1];
                break;
            }
        }
        if (config_path.empty()) {
            namespace fs = std::filesystem;
            auto candidate = fs::path(Config::SOURCE_DIR) / "mayaflux.json";
            if (fs::exists(candidate))
                config_path = candidate.string();
        }
        if (!config_path.empty())
            MayaFlux::Config::load_config_from_file(config_path);

        initialize();

        MayaFlux::Init();

        MayaFlux::Start();

        MF_LOG(Journal::Component::USER, Journal::Context::Init, "=== AudioVisual Processing Active ===");

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
