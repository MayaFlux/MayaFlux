#include "Runtime.hpp"

#include "Host/Dependencies.hpp"

#include "Lila/Commentator.hpp"

namespace MayaFlux::Host::Live {

namespace {

    std::optional<std::filesystem::path> scan_for_directory(const std::string& filename)
    {
        const auto cwd = std::filesystem::current_path();
        for (const auto& entry : std::filesystem::recursive_directory_iterator(cwd)) {
            if (entry.is_regular_file() && entry.path().filename() == filename) {
                return entry.path().parent_path();
            }
        }
        return std::nullopt;
    }

    void warn(const std::string& call, const Failure& failure)
    {
        LILA_WARN(Lila::Emitter::SYSTEM, call + ": " + failure.message);
    }

}

bool start_lila(uint16_t port)
{
    auto started = start_session(port);
    if (!started) {
        LILA_ERROR(Lila::Emitter::SYSTEM, "start_lila: " + started.error().message);
    }
    return started.has_value();
}

void stop_lila(bool clear_persistent_store)
{
    stop_session(clear_persistent_store);
}

bool lila_active()
{
    return session_active();
}

uint16_t lila_port()
{
    return session_port();
}

std::optional<std::filesystem::path> find_include_dir(const std::string& filename)
{
    return scan_for_directory(filename);
}

void lila_add_include_path(const std::string& path)
{
    if (auto added = add_include_root(path); !added) {
        warn("lila_add_include_path", added.error());
    }
}

void lila_add_compile_flag(const std::string& flag)
{
    if (auto added = add_compile_flag(flag); !added) {
        warn("lila_add_compile_flag", added.error());
    }
}

bool lila_add_header(const std::string& filename)
{
    auto dir = scan_for_directory(filename);
    if (!dir) {
        LILA_WARN(Lila::Emitter::SYSTEM,
            "lila_add_header: could not find " + filename);
        return false;
    }

    if (auto added = add_include_root(*dir); !added) {
        warn("lila_add_header", added.error());
    }

    auto included = add_header(filename, std::span<const std::filesystem::path>(&*dir, 1));
    if (!included) {
        warn("lila_add_header", included.error());
    }
    return included.has_value();
}

void lila_load_library(const std::string& path)
{
    if (auto loaded = load_library(path); !loaded) {
        warn("lila_load_library", loaded.error());
    }
}

bool lila_eval(const std::string& code)
{
    auto evaluated = eval(code);
    if (!evaluated) {
        warn("lila_eval", evaluated.error());
    }
    return evaluated.has_value();
}

} // namespace MayaFlux::Host::Live
