#pragma once

#ifdef MAYAFLUX_PROJECT
#ifndef MAYAFLUX_DEVELOPMENT
#include "pch.h"
#endif // MAYAFLUX_DEVELOPMENT
#endif // MAYAFLUX_PROJECT

#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/API/Core.hpp"

#include "MayaFlux/API/Graph.hpp"

#include "MayaFlux/API/Chronie.hpp"

#include "MayaFlux/API/Depot.hpp"

#include "MayaFlux/API/Random.hpp"

#include "MayaFlux/API/Windowing.hpp"

#include "MayaFlux/API/Yantra.hpp"

#ifdef MAYASIMPLE
#include "MayaFlux/API/Proxy/Creator.hpp"

#include "Nodes/Filters/FIR.hpp"
#include "Nodes/Filters/IIR.hpp"
#include "Nodes/Generators/Impulse.hpp"
#include "Nodes/Generators/Phasor.hpp"
#include "Nodes/Generators/Sine.hpp"
#include "Nodes/Generators/Stochastic.hpp"
#include "Nodes/Graphics/ComputeOutNode.hpp"
#include "Nodes/Graphics/GeometryWriterNode.hpp"
#include "Nodes/Graphics/PointCollectionNode.hpp"
#include "Nodes/Graphics/PointNode.hpp"
#include "Nodes/Graphics/ProceduralTextureNode.hpp"
#include "Nodes/Graphics/TextureNode.hpp"
#include "Nodes/Network/NodeNetwork.hpp"
#include "Nodes/NodeGraphManager.hpp"
#include "Nodes/NodeStructure.hpp"

#include "MayaFlux/Nodes/Network/ModalNetwork.hpp"
#include "MayaFlux/Nodes/Network/ParticleNetwork.hpp"

#include "Buffers/BufferManager.hpp"
#include "Buffers/BufferProcessingChain.hpp"
#include "Buffers/Container/SoundContainerBuffer.hpp"
#include "Buffers/Container/SoundStreamWriter.hpp"
#include "Buffers/Geometry/GeometryBuffer.hpp"
#include "Buffers/Network/NetworkGeometryBuffer.hpp"
#include "Buffers/Node/FilterProcessor.hpp"
#include "Buffers/Node/LogicProcessor.hpp"
#include "Buffers/Node/NodeBindingsProcessor.hpp"
#include "Buffers/Node/NodeBuffer.hpp"
#include "Buffers/Node/PolynomialProcessor.hpp"
#include "Buffers/Recursive/FeedbackBuffer.hpp"
#include "Buffers/Shaders/DescriptorBindingsProcessor.hpp"
#include "Buffers/Shaders/RenderProcessor.hpp"
#include "Buffers/Staging/BufferDownloadProcessor.hpp"
#include "Buffers/Staging/BufferUploadProcessor.hpp"
#include "Buffers/Textures/NodeTextureBuffer.hpp"
#include "Buffers/Textures/TextureBuffer.hpp"

#include "Kriya/Awaiters/DelayAwaiters.hpp"
#include "Kriya/Awaiters/EventAwaiter.hpp"
#include "Kriya/BufferPipeline.hpp"
#include "Kriya/Chain.hpp"
#include "Kriya/CycleCoordinator.hpp"
#include "Kriya/InputEvents.hpp"
#include "Kriya/Tasks.hpp"

#include "Vruta/Event.hpp"
#include "Vruta/EventManager.hpp"
#include "Vruta/EventSource.hpp"

#include "Kakshya/Source/DynamicSoundStream.hpp"
#include "Kakshya/Source/SoundFileContainer.hpp"

#include "Journal/Archivist.hpp"

#include "Core/Windowing/WindowManager.hpp"

#include "Core/GlobalGraphicsInfo.hpp"
#include "Core/GlobalStreamInfo.hpp"

#include "Portal/Graphics/Graphics.hpp"
#include "Portal/Graphics/SamplerForge.hpp"
#include "Portal/Graphics/ShaderFoundry.hpp"
#include "Portal/Graphics/TextureLoom.hpp"

#include "MayaFlux/IO/ImageReader.hpp"

using namespace MayaFlux::Kakshya;
using namespace MayaFlux::Kriya;
using namespace MayaFlux::Buffers;
using namespace MayaFlux::Nodes::GpuSync;
using namespace MayaFlux::Nodes::Network;
using namespace MayaFlux::Nodes::Filters;
using namespace MayaFlux::Nodes::Generator;
using namespace MayaFlux::Nodes;
using namespace MayaFlux;

#endif // MAYASIMPLE

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
