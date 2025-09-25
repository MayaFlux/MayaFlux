#pragma once

#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/API/Chronie.hpp"

#include "MayaFlux/API/Depot.hpp"

#include "MayaFlux/API/Random.hpp"

#include "MayaFlux/API/Yantra.hpp"

#ifdef MAYASIMPLE
#include "MayaFlux/API/Proxy/Creator.hpp"

#include "Kriya/Bridge.hpp"

#include "Nodes/Filters/FIR.hpp"
#include "Nodes/Filters/IIR.hpp"
#include "Nodes/Generators/Impulse.hpp"
#include "Nodes/Generators/Phasor.hpp"
#include "Nodes/Generators/Sine.hpp"
#include "Nodes/Generators/Stochastic.hpp"
#include "Nodes/NodeGraphManager.hpp"
#include "Nodes/NodeStructure.hpp"

#include "Buffers/BufferProcessingChain.hpp"
#include "Buffers/Container/ContainerBuffer.hpp"
#include "Buffers/Container/StreamWriteProcessor.hpp"
#include "Buffers/Node/LogicProcessor.hpp"
#include "Buffers/Node/NodeBuffer.hpp"
#include "Buffers/Node/PolynomialProcessor.hpp"
#include "Buffers/Recursive/FeedbackBuffer.hpp"

#include "Kriya/Bridge.hpp"
#include "Kriya/Chain.hpp"
#include "Kriya/Tasks.hpp"

#include "Kakshya/Source/DynamicSoundStream.hpp"
#include "Kakshya/Source/SoundFileContainer.hpp"

using namespace MayaFlux::Kakshya;
using namespace MayaFlux::Kriya;
using namespace MayaFlux::Buffers;
using namespace MayaFlux::Nodes::Generator;
using namespace MayaFlux::Nodes;
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
