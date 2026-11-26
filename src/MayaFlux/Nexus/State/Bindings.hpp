#pragma once

#include "MayaFlux/Transitive/Reflect/FunctionTable.hpp"

namespace MayaFlux::Buffers {
class VKBuffer;
}

namespace MayaFlux::Core {
class Window;
}

namespace MayaFlux::Kriya {
class Course;
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
 * @code
 * auto bindings = std::make_shared<Nexus::State::Bindings>();
 * bindings->buffers["wolf"] = wolf;
 * bindings->assets["wolf"] = { .file = "res/wolf.glb", .mesh = 0 };
 * bindings->windows["main"] = window;
 * bindings->functions.expose<&Nexus::follow_buffer>();
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
};

}
