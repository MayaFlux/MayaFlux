#include "Session.hpp"

#include "Lila/Commentator.hpp"
#include "Lila/Lila.hpp"

#include "MayaFlux/Transitive/Memory/Persist.hpp"
#include "MayaFlux/Transitive/Memory/SnapshotCell.hpp"
#include "MayaFlux/Transitive/Platform/Acquisition.hpp"

#ifdef MAYAFLUX_PLATFORM_WINDOWS
#include "MayaFlux/Transitive/Parallel/Dispatch.hpp"
#endif

namespace MayaFlux::Host {

namespace {

    constexpr size_t k_stage_count = 4;

    struct State {
        std::shared_ptr<Lila::Lila> instance;
        std::optional<Lila::Address> address;
        bool starting {};
        std::array<std::vector<std::string>, k_stage_count> staged;
        std::set<std::string> libraries;
    };

    enum class Outcome : uint8_t {
        Staged,
        Duplicate,
        Running,
        Starting
    };

    Memory::SnapshotCell<State> g_state;

    std::unexpected<Failure> refuse(std::string message, std::vector<std::filesystem::path> tried = {})
    {
        return std::unexpected(Failure { .message = std::move(message), .tried = std::move(tried) });
    }

    size_t index_of(detail::Stage stage)
    {
        return static_cast<size_t>(stage);
    }

    std::string where_text(const std::optional<Lila::Address>& address)
    {
        if (!address) {
            return "nowhere";
        }
        if (const auto* tcp = std::get_if<Lila::TcpAddress>(&*address)) {
            return tcp->host + ":" + std::to_string(tcp->port);
        }
        return "socket " + std::get<Lila::UnixAddress>(*address).path.value_or(std::filesystem::path {}).string();
    }

    std::shared_ptr<Lila::Lila> running_instance()
    {
        auto view = g_state.read();
        return view ? view->instance : nullptr;
    }

    bool library_known(const std::string& path)
    {
        auto view = g_state.read();
        return view && view->libraries.contains(path);
    }

    Result<void> run_eval(Lila::Lila& lila, const std::string& code)
    {
        if (auto evaluated = lila.eval(code); !evaluated) {
            return refuse(std::string(evaluated.error()));
        }
        return {};
    }

    Result<void> run_load(Lila::Lila& lila, const std::string& path)
    {
        if (auto loaded = lila.load_library(path); !loaded) {
            return refuse(std::string(loaded.error()), { path });
        }
        return {};
    }

    Result<void> apply_now(Lila::Lila& lila, detail::Stage stage, const std::string& value)
    {
        if (stage == detail::Stage::Eval) {
            return run_eval(lila, value);
        }

        if (stage == detail::Stage::Library) {
            if (library_known(value)) {
                return {};
            }
            auto loaded = run_load(lila, value);
            if (loaded) {
                g_state.update([&value](State& state) { state.libraries.insert(value); });
            }
            return loaded;
        }

        return refuse("takes effect only before start_session: " + value);
    }

}

namespace detail {

    Result<void> apply_or_stage(Stage stage, std::string value)
    {
        for (;;) {
            Outcome outcome = Outcome::Staged;

            g_state.update([&](State& state) {
                if (state.instance) {
                    outcome = Outcome::Running;
                    return false;
                }
                if (state.starting) {
                    outcome = Outcome::Starting;
                    return false;
                }
                if (stage == Stage::Library && !state.libraries.insert(value).second) {
                    outcome = Outcome::Duplicate;
                    return false;
                }
                state.staged[index_of(stage)].push_back(value);
                outcome = Outcome::Staged;
                return true;
            });

            if (outcome == Outcome::Staged || outcome == Outcome::Duplicate) {
                return {};
            }
            if (outcome == Outcome::Starting) {
                return refuse("session is starting");
            }

            if (auto instance = running_instance()) {
                return apply_now(*instance, stage, value);
            }
        }
    }

    Failure to_failure(const Platform::AcquireFailure& failure)
    {
        using Reason = Platform::AcquireFailure::Reason;

        std::string message;
        switch (failure.reason) {
        case Reason::EmptyRequest:
            message = "empty request";
            break;
        case Reason::InvalidName:
            message = "invalid name";
            break;
        case Reason::Missing:
            message = "not found";
            break;
        case Reason::NotDirectory:
            message = "not a directory";
            break;
        case Reason::NotRegularFile:
            message = "not a regular file";
            break;
        case Reason::MarkerMissing:
            message = "marker file missing";
            break;
        case Reason::TooLong:
            message = "path too long";
            break;
        case Reason::Unsafe:
            message = "unsafe location";
            break;
        case Reason::NotSocket:
            message = "not a socket";
            break;
        case Reason::InUse:
            message = "already in use";
            break;
        case Reason::StaleNotRemoved:
            message = "stale entry could not be removed";
            break;
        }
        if (!failure.request.empty()) {
            message += ": " + failure.request;
        }
        return Failure { .message = std::move(message), .tried = failure.tried };
    }

}

Result<void> start_session(const Lila::Address& where)
{
    enum class Refusal : uint8_t {
        None,
        Running,
        Starting
    };

    Refusal refusal = Refusal::None;
    std::optional<Lila::Address> running;
    std::array<std::vector<std::string>, k_stage_count> staged;

    g_state.update([&](State& state) {
        if (state.instance) {
            refusal = Refusal::Running;
            running = state.address;
            return false;
        }
        if (state.starting) {
            refusal = Refusal::Starting;
            return false;
        }
        refusal = Refusal::None;
        staged = std::exchange(state.staged, {});
        state.starting = true;
        return true;
    });

    if (refusal == Refusal::Running) {
        return refuse("session already running on " + where_text(running));
    }
    if (refusal == Refusal::Starting) {
        return refuse("session is starting");
    }

    auto instance = std::make_shared<Lila::Lila>();

    for (const auto& path : staged[index_of(detail::Stage::IncludePath)]) {
        instance->add_include_path(path);
    }
    for (const auto& flag : staged[index_of(detail::Stage::CompileFlag)]) {
        instance->add_compile_flag(flag);
    }

    const auto abandon = [&instance](Failure failure) -> Result<void> {
        instance->stop_listening();
        g_state.update([](State& state) {
            state.starting = false;
            state.libraries.clear();
        });
        return std::unexpected(std::move(failure));
    };

    if (const auto ready = instance->initialize(true); !ready) {
        return abandon(Failure { .message = "initialization failed: " + std::string(ready.error()), .tried = {} });
    }

    for (const auto& library : staged[index_of(detail::Stage::Library)]) {
        if (auto loaded = run_load(*instance, library); !loaded) {
            return abandon(std::move(loaded.error()));
        }
    }
    for (const auto& code : staged[index_of(detail::Stage::Eval)]) {
        if (auto evaluated = run_eval(*instance, code); !evaluated) {
            return abandon(std::move(evaluated.error()));
        }
    }

    if (const auto listening = instance->listen(where); !listening) {
        return abandon(Failure { .message = "listen failed: " + std::string(listening.error()), .tried = {} });
    }

    const auto* resolved = instance->address();
    const std::optional<Lila::Address> bound = resolved ? std::optional<Lila::Address>(*resolved) : std::nullopt;
    g_state.update([&](State& state) {
        state.instance = instance;
        state.address = bound;
        state.starting = false;
    });

    LILA_INFO(Lila::Emitter::SYSTEM, "start_session: running on " + where_text(bound));
    return {};
}

void stop_session(bool clear_persistent_store)
{
    std::shared_ptr<Lila::Lila> instance;
    std::optional<Lila::Address> stopped;

    g_state.update([&](State& state) {
        if (!state.instance) {
            instance.reset();
            return false;
        }
        instance = std::move(state.instance);
        stopped = std::exchange(state.address, std::nullopt);
        state.libraries.clear();
        return true;
    });

    if (!instance) {
        return;
    }

    LILA_INFO(Lila::Emitter::SYSTEM,
        "stop_session: stopping on " + where_text(stopped));

    instance->stop_listening();
    instance.reset();

    if (clear_persistent_store) {
        MayaFlux::internal::cleanup_persistent_store();
        LILA_DEBUG(Lila::Emitter::SYSTEM, "stop_session: cleared persistent store");
    }
}

bool session_active()
{
    auto view = g_state.read();
    return view && view->instance;
}

std::optional<Lila::Address> session_address()
{
    auto view = g_state.read();
    return view && view->instance ? view->address : std::nullopt;
}

Result<void> eval(const std::string& code)
{
    return detail::apply_or_stage(detail::Stage::Eval, code);
}

Result<void> eval_file(const std::filesystem::path& path)
{
    auto located = Platform::acquire_library(Platform::LibraryFromPath { .path = path });
    if (!located) {
        return std::unexpected(detail::to_failure(located.error()));
    }

    const std::string file = located->path.generic_string();
    if (file.find_first_of("\"\r\n") != std::string::npos) {
        return refuse("path cannot be included: " + file);
    }

    return eval("#include \"" + file + "\"");
}

Result<void*> symbol_address(const std::string& name)
{
    auto instance = running_instance();
    if (!instance) {
        return refuse("no active session");
    }

    void* address = instance->get_symbol_address(name);
    if (!address) {
        return refuse("symbol not found: " + name);
    }
    return address;
}

} // namespace MayaFlux::Host
