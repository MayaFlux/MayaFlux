#pragma once

/**
 * @file MayaFlux.hpp
 * @brief Single include for the user facing API.
 *
 * Includes the API headers (Config, Core, Graph, Chronie, Depot, Input, Random,
 * Windowing, ViewportPreset, Creator, Temporal, Rigs) and the persistence
 * helpers. Unless MAYAFLUX_NO_CONVENIENCE is defined it also includes
 * Convenience.hpp, which adds the everyday headers and namespace imports.
 *
 * SimulationIncludes.hpp and WorkflowIncludes.hpp are not included here.
 * Include them explicitly, and define their MAYAFLUX_*_NO_* macros first to
 * leave a group out.
 */

#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/API/Chronie.hpp"

#include "MayaFlux/API/Depot.hpp"

#include "MayaFlux/API/Input.hpp"

#include "MayaFlux/API/Random.hpp"

#include "MayaFlux/API/Windowing.hpp"

#include "MayaFlux/API/ViewportPreset.hpp"

#include "MayaFlux/API/Proxy/Creator.hpp"

#include "MayaFlux/API/Proxy/Temporal.hpp"

#include "MayaFlux/API/Rigs.hpp"

#include "MayaFlux/Transitive/Memory/Persist.hpp"

#if (!defined(MAYAFLUX_NO_CONVENIENCE))
#include "Convenience.hpp"
#endif

/**
 * @namespace MayaFlux
 * @brief Main namespace for the Maya Flux audio engine
 *
 * This namespace provides convenience wrappers around the core functionality of
 * the Maya Flux audio engine. These wrappers simplify access to the centrally
 * managed components and common operations, making it easier to work with the
 * engine without directly managing the Engine instance.
 *
 * All functions in this namespace operate on the default Engine instance and
 * its managed components. For custom or non-default components, use their
 * specific handles and methods directly rather than these wrappers.
 */
namespace MayaFlux {
}
