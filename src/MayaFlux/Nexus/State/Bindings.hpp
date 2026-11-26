#pragma once

#include "MayaFlux/Transitive/Reflect/FunctionTable.hpp"

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
 * @struct Bindings
 * @brief What a saved state refers to by name and cannot hold itself.
 *
 * The caller owns it and gives it to StateEncoder and StateDecoder. Saving looks
 * an object up to write its name, loading looks the name up to get an object, so
 * a loading program binds the same names to its own objects. Nothing is bound in
 * advance.
 *
 * @code
 * auto bindings = std::make_shared<Nexus::State::Bindings>();
 * bindings->buffers["wolf"] = wolf;
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
};

}
