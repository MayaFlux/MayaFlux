#pragma once

#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/API/Chronie.hpp"

#include "MayaFlux/API/Depot.hpp"

#include "MayaFlux/API/Random.hpp"

#ifdef MAYASIMPLE
#include "MayaFlux/API/Proxy/Creator.hpp"

using namespace MayaFlux::Kakshya;
using namespace MayaFlux::Kriya;
using namespace MayaFlux::Buffers;
using namespace MayaFlux;

#endif // MAYASIMPLE

/**
 * @namespace MayaFlux
 * @brief Main namespace for the Maya Flux audio engine
 *
 * This namespace provides convenience wrappers around the core functionality of the Maya Flux
 * audio engine. These wrappers simplify access to the centrally managed components and common
 * operations, making it easier to work with the engine without directly managing the Engine
 * instance.
 *
 * All functions in this namespace operate on the default Engine instance and its managed
 * components. For custom or non-default components, use their specific handles and methods
 * directly rather than these wrappers.
 */
namespace MayaFlux {

}
