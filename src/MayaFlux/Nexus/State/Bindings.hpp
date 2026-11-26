#pragma once

#include "MayaFlux/Kriya/Course.hpp"
#include "MayaFlux/Nexus/Fabric.hpp"
#include "MayaFlux/Transitive/Reflect/FunctionTable.hpp"

#include <typeindex>

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Vruta {
class NetworkSource;
}

namespace MayaFlux::Nexus::State {

using BufferNames = std::map<std::string, std::shared_ptr<Buffers::VKBuffer>, std::less<>>;
using WindowNames = std::map<std::string, std::shared_ptr<Core::Window>, std::less<>>;
using SourceNames = std::map<std::string, std::shared_ptr<Vruta::NetworkSource>, std::less<>>;

/** @brief Described functions whose arguments may name a buffer, window or network source. */
using Functions = Reflect::FunctionTable<BufferNames, WindowNames, SourceNames>;

/**
 * @struct AssetSource
 * @brief The file a bound buffer was loaded from, and which part of it.
 *
 * Declaring one says the buffer is still what the file holds, so saving refers
 * to the file instead of writing a snapshot.
 */
struct AssetSource {
    std::string file;
    std::optional<uint64_t> row; ///< A row of the file's assembly, as StateOrchestrator realizes it.
    uint32_t mesh {}; ///< Without a row, the index among ModelReader::create_mesh_buffers().
    Reflect::FunctionCall make {}; ///< With a name, the buffer is made by this call instead of read from a file.
};

/** @brief Anything a struct of the user's own controls can be added to. */
using Extendable = std::variant<Emitter*, Sensor*, Agent*, Expanse*, Kriya::Course*>;

/** @brief A subclass of Agent a saved state can make again. */
struct AgentClass {
    std::type_index type { typeid(void) };
    std::function<std::shared_ptr<Agent>()> make;
};

/** @brief A subclass of Expanse a saved state can make again. */
struct ExpanseClass {
    std::type_index type { typeid(void) };
    std::function<std::shared_ptr<Expanse>()> make;
};

/**
 * @struct Bindings
 * @brief What a saved state refers to by name and cannot hold itself.
 *
 * The caller owns it and gives it to StateEncoder and StateDecoder. Saving looks
 * an object up to write its name, loading looks the name up to get an object, so
 * a loading program binds the same names to its own objects. Nothing is bound in
 * advance.
 *
 * Every bound buffer is part of the saved state. One with an entry in assets is
 * saved as that file reference; otherwise a MeshBuffer or MeshNetworkBuffer is
 * written as a snapshot beside the state. A loading program that binds the name
 * keeps its own buffer, and one that does not gets the buffer loaded for it in
 * StateDecoder::ReconstructionResult::buffers, neither registered nor drawn.
 *
 * A bound course has its playback saved: its numeric controls, range, time,
 * whether it is paused and its markers. Loading applies that to the course bound
 * under the same name and never makes one; the commands take effect on its next
 * advance. A time map or repeat, and so following another course, is a closure
 * and is not saved.
 *
 * A struct added with extend() is only restored when registered with
 * register_controls(), and a subclass of Agent or Expanse only when registered
 * with register_class(). A function that sets something up through configure(),
 * on a principal, a Wiring or a course, is made from functions like any other.
 *
 * @code
 * auto bindings = std::make_shared<Nexus::State::Bindings>();
 * bindings->buffers["wolf"] = wolf;
 * bindings->assets["wolf"] = { .file = "res/wolf.glb", .mesh = 0 };
 * bindings->windows["main"] = window;
 * bindings->functions.expose<&Nexus::follow_buffer>();
 * bindings->register_controls<WolfControls>();
 * bindings->register_class<Wolf>("wolf", [] { return std::make_shared<Wolf>(); });
 *
 * Nexus::StateEncoder encoder(bindings);
 * Nexus::StateDecoder decoder(bindings);
 * @endcode
 */
struct Bindings {
    BufferNames buffers;
    WindowNames windows;
    SourceNames sources;
    Functions functions;
    std::map<std::string, AssetSource, std::less<>> assets;
    std::map<std::string, std::shared_ptr<Kriya::Course>, std::less<>> courses;

    std::map<std::string, std::function<void(const Extendable&)>, std::less<>> controls;
    std::map<std::string, AgentClass, std::less<>> agents;
    std::map<std::string, ExpanseClass, std::less<>> expanses;

    /**
     * @brief Let a loaded state add a described controls struct back to whatever held it.
     * @tparam C Struct given to extend(), found by its unqualified name.
     */
    template <typename C>
    void register_controls()
    {
        const std::string_view full = Reflect::Generated::Description<C>::name;
        const size_t scope = full.rfind("::");
        controls.insert_or_assign(
            std::string(scope == std::string_view::npos ? full : full.substr(scope + 2)),
            [](const Extendable& target) {
                std::visit([](auto* host) {
                    if constexpr (requires { host->template extend<C>(); }) {
                        static_cast<void>(host->template extend<C>());
                    }
                },
                    target);
            });
    }

    /**
     * @brief Name a subclass of Agent or Expanse, so a loaded state gets it back.
     * @param name  Written into the state for every object of exactly this type.
     * @param make  Builds one, with the functions it needs.
     */
    template <typename T>
        requires(std::is_base_of_v<Agent, T> || std::is_base_of_v<Expanse, T>)
    void register_class(std::string name, std::function<std::shared_ptr<T>()> make)
    {
        if constexpr (std::is_base_of_v<Agent, T>) {
            agents.insert_or_assign(std::move(name), AgentClass { std::type_index(typeid(T)), std::move(make) });
        } else {
            expanses.insert_or_assign(std::move(name), ExpanseClass { std::type_index(typeid(T)), std::move(make) });
        }
    }
};

}
