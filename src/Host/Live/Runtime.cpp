#include "Runtime.hpp"

#include "Lila/Commentator.hpp"
#include "Lila/Lila.hpp"

#include "MayaFlux/Transitive/Memory/Persist.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"
#endif

namespace MayaFlux::Host::Live {

namespace {
    std::mutex g_mutex;
    std::unique_ptr<Lila::Lila> g_instance;
    uint16_t g_port {};
    std::vector<std::string> g_pending_include_paths;
    std::vector<std::string> g_pending_compile_flags;
    std::vector<std::string> g_pending_libraries;
    std::vector<std::string> g_pending_evals;
}

bool start_lila(uint16_t port)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (g_instance) {
        LILA_WARN(Lila::Emitter::SYSTEM,
            "start_lila: already running on port " + std::to_string(g_port));
        return false;
    }

    auto instance = std::make_unique<Lila::Lila>();

    for (const auto& p : g_pending_include_paths)
        instance->add_include_path(p);
    g_pending_include_paths.clear();

    for (const auto& f : g_pending_compile_flags)
        instance->add_compile_flag(f);
    g_pending_compile_flags.clear();

    if (!instance->initialize(Lila::OperationMode::Both, static_cast<int>(port), true)) {
        LILA_ERROR(Lila::Emitter::SYSTEM,
            "start_lila: initialization failed: " + instance->get_last_error());
        return false;
    }

    for (const auto& lib : g_pending_libraries)
        instance->load_library(lib);
    g_pending_libraries.clear();

    for (const auto& code : g_pending_evals)
        instance->eval(code);
    g_pending_evals.clear();

    g_instance = std::move(instance);
    g_port = port;

    LILA_INFO(Lila::Emitter::SYSTEM,
        "start_lila: running on port " + std::to_string(port));
    return true;
}

void stop_lila(bool clear_persistent_store)
{
    std::lock_guard<std::mutex> guard(g_mutex);

    if (!g_instance) {
        return;
    }

    LILA_INFO(Lila::Emitter::SYSTEM,
        "stop_lila: stopping on port " + std::to_string(g_port));

    g_instance->stop_server();
    g_instance.reset();
    g_port = 0;

    if (clear_persistent_store) {
        internal::cleanup_persistent_store();
        LILA_DEBUG(Lila::Emitter::SYSTEM, "stop_lila: cleared persistent store");
    }
}

bool lila_active()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return static_cast<bool>(g_instance);
}

uint16_t lila_port()
{
    std::lock_guard<std::mutex> guard(g_mutex);
    return g_instance ? g_port : uint16_t {};
}

std::optional<std::filesystem::path> find_include_dir(const std::string& filename)
{
    const auto cwd = std::filesystem::current_path();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cwd)) {
        if (entry.is_regular_file() && entry.path().filename() == filename) {
            return entry.path().parent_path();
        }
    }
    return std::nullopt;
}

void lila_add_include_path(const std::string& path)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_instance) {
        g_instance->add_include_path(path);
        return;
    }
    g_pending_include_paths.push_back(path);
}

void lila_add_compile_flag(const std::string& flag)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_instance) {
        g_instance->add_compile_flag(flag);
        return;
    }
    g_pending_compile_flags.push_back(flag);
}

bool lila_add_header(const std::string& filename)
{
    auto dir = find_include_dir(filename);
    if (!dir) {
        LILA_WARN(Lila::Emitter::SYSTEM,
            "lila_add_header: could not find " + filename);
        return false;
    }
    lila_add_include_path(dir->string());
    lila_eval("#include \"" + filename + "\"");
    return true;
}

void lila_load_library(const std::string& path)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_instance) {
        g_instance->load_library(path);
        return;
    }
    g_pending_libraries.push_back(path);
}

bool lila_eval(const std::string& code)
{
    std::lock_guard<std::mutex> guard(g_mutex);
    if (g_instance) {
        return g_instance->eval(code);
    }
    g_pending_evals.push_back(code);
    return true;
}

} // namespace MayaFlux::Host::Live
