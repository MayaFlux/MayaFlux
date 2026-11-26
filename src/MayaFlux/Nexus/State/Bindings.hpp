#pragma once

#include "MayaFlux/Kinesis/VertexSampler.hpp"
#include "MayaFlux/Kriya/Course.hpp"
#include "MayaFlux/Nexus/Fabric.hpp"
#include "MayaFlux/Transitive/Reflect/FunctionTable.hpp"

#include <typeindex>

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Kakshya {
class MeshAssemblyContainer;
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Portal::Graphics {
class ImageTarget;
}

namespace MayaFlux::Vruta {
class NetworkSource;
}

namespace MayaFlux::Nexus::State {

using BufferNames = std::map<std::string, std::shared_ptr<Buffers::VKBuffer>, std::less<>>;
using WindowNames = std::map<std::string, std::shared_ptr<Core::Window>, std::less<>>;
using SourceNames = std::map<std::string, std::shared_ptr<Vruta::NetworkSource>, std::less<>>;
using CourseNames = std::map<std::string, std::shared_ptr<Kriya::Course>, std::less<>>;

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
    Reflect::FunctionCall realize {}; ///< With a name, makes the options a row is realized with.
    std::optional<Kinesis::SamplerBounds> realize_bounds {}; ///< The bounds of those options.
};

/** @brief Rows and the principals a motion carries because nothing draws them. None are retained. */
using Carried = std::vector<std::pair<uint64_t,
    std::variant<std::weak_ptr<Emitter>, std::weak_ptr<Sensor>, std::weak_ptr<Agent>>>>;

/**
 * @struct MotionSource
 * @brief A motion a StateOrchestrator started, so a saved state can start it again.
 */
struct MotionSource {
    std::string file; ///< The assembly's file, empty for one made in code, which is written beside the state.
    std::shared_ptr<const Kakshya::MeshAssemblyContainer> assembly;
    uint64_t motion {};
    Kriya::CourseEdge edge { Kriya::CourseEdge::Loop };
    std::vector<std::string> courses; ///< Names in courses, in the order the reflows hold them.
    Carried carried;
    std::vector<std::string> buffers {}; ///< Names in buffers this motion moves.
    std::vector<uint64_t> rows {}; ///< The row each of those shows.
    Reflect::FunctionCall realize {}; ///< With a name, makes the options the assembly is realized with.
    std::optional<Kinesis::SamplerBounds> realize_bounds {}; ///< The bounds of those options.
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
 * saved as that file reference; otherwise it is written as a snapshot beside the
 * state, replacing the one an earlier save to the same place wrote: meshes and mesh
 * networks as models, instances as their template and slot values, volumes as VDB,
 * textures as images, and point and curve buffers as Alembic, which load as
 * RecordedGeometryBuffers holding the saved moment. A loading program that binds the name
 * keeps its own buffer, and one that does not gets the buffer loaded for it in
 * StateDecoder::ReconstructionResult::buffers, neither registered nor drawn.
 *
 * A bound course has its playback saved: its numeric controls, range, markers,
 * where its clock is, its direction, counts and fade, and the bound course it
 * follows. Loading applies that to the course bound under the same name, or makes
 * one and answers it in ReconstructionResult::courses; the commands take effect on
 * its next advance. A time map or repeat given as a closure is saved only through
 * configure().
 *
 * A motion a StateOrchestrator given these bindings started is saved with its
 * courses, and loading starts it again and answers it in
 * ReconstructionResult::motions. A motion whose name is bound already is left to
 * the loading program.
 *
 * A render sink's image targets are saved by the names in targets.
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
    CourseNames courses;
    std::map<std::string, MotionSource, std::less<>> motions;
    std::map<std::string, std::shared_ptr<Portal::Graphics::ImageTarget>, std::less<>> targets;

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
