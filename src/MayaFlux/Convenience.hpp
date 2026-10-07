#pragma once

/**
 * @file Convenience.hpp
 * @brief Everyday headers and namespace imports for user code.
 *
 * Includes the node, buffer, task, container, geometry, window, portal and IO
 * headers a typical project touches, then opens their namespaces so names such
 * as Sine, AudioBuffer or SamplingPipeline need no qualification.
 *
 * MayaFlux.hpp includes this file unless MAYAFLUX_NO_CONVENIENCE is defined.
 * Define it to keep the global namespace clean and include only what you use.
 * Simulation and compute headers are separate, see SimulationIncludes.hpp and
 * WorkflowIncludes.hpp.
 *
 * @note The using directives apply to the including translation unit. Library
 *       headers must never include this file.
 */

#include "Nodes/Conduit/Constant.hpp"
#include "Nodes/Conduit/NodeChain.hpp"
#include "Nodes/Conduit/NodeCombine.hpp"
#include "Nodes/Conduit/StreamReaderNode.hpp"
#include "Nodes/Filters/FIR.hpp"
#include "Nodes/Filters/IIR.hpp"
#include "Nodes/Generators/Counter.hpp"
#include "Nodes/Generators/Impulse.hpp"
#include "Nodes/Generators/Phasor.hpp"
#include "Nodes/Generators/Random.hpp"
#include "Nodes/Generators/Sine.hpp"
#include "Nodes/Graphics/GeometryLeafNode.hpp"
#include "Nodes/Graphics/GeometryReadbackNode.hpp"
#include "Nodes/Graphics/GeometryWriterNode.hpp"
#include "Nodes/Graphics/GlyphGeometryNode.hpp"
#include "Nodes/Graphics/LineSegmentsNode.hpp"
#include "Nodes/Graphics/MeshWriterNode.hpp"
#include "Nodes/Graphics/PathGeneratorNode.hpp"
#include "Nodes/Graphics/PointCollectionNode.hpp"
#include "Nodes/Graphics/ProceduralTextureNode.hpp"
#include "Nodes/Graphics/SDFNode.hpp"
#include "Nodes/Graphics/TextureNode.hpp"
#include "Nodes/Graphics/TopologyGeneratorNode.hpp"
#include "Nodes/Network/NodeNetwork.hpp"
#include "Nodes/NodeGraphManager.hpp"

#include "Nodes/Network/AssemblyNetwork.hpp"
#include "Nodes/Network/InstanceNetwork.hpp"
#include "Nodes/Network/MeshNetwork.hpp"
#include "Nodes/Network/ModalNetwork.hpp"
#include "Nodes/Network/ParticleNetwork.hpp"
#include "Nodes/Network/PointCloudNetwork.hpp"
#include "Nodes/Network/RelationNetwork.hpp"
#include "Nodes/Network/ResonatorNetwork.hpp"
#include "Nodes/Network/WaveguideNetwork.hpp"

#include "Nodes/Input/HIDNode.hpp"
#include "Nodes/Input/MIDINode.hpp"
#include "Nodes/Input/OSCNode.hpp"
#include "Nodes/Input/TabletNode.hpp"

#include "Buffers/BufferManager.hpp"
#include "Buffers/BufferProcessingChain.hpp"
#include "Buffers/Container/SoundContainerBuffer.hpp"
#include "Buffers/Container/SoundStreamWriter.hpp"
#include "Buffers/Container/VideoContainerBuffer.hpp"
#include "Buffers/Forma/FormaBindingsProcessor.hpp"
#include "Buffers/Forma/FormaBuffer.hpp"
#include "Buffers/Geometry/CompositeGeometryBuffer.hpp"
#include "Buffers/Geometry/ComputeMeshBuffer.hpp"
#include "Buffers/Geometry/GeometryBuffer.hpp"
#include "Buffers/Geometry/MeshBuffer.hpp"
#include "Buffers/Network/InstanceNetworkBuffer.hpp"
#include "Buffers/Network/MeshNetworkBuffer.hpp"
#include "Buffers/Network/NetworkAudioBuffer.hpp"
#include "Buffers/Network/NetworkGeometryBuffer.hpp"
#include "Buffers/Network/NetworkTextureBuffer.hpp"
#include "Buffers/Node/FilterProcessor.hpp"
#include "Buffers/Node/LogicProcessor.hpp"
#include "Buffers/Node/NodeBindingsProcessor.hpp"
#include "Buffers/Node/NodeBuffer.hpp"
#include "Buffers/Node/NodeFeedProcessor.hpp"
#include "Buffers/Node/PolynomialProcessor.hpp"
#include "Buffers/Recursive/FeedbackBuffer.hpp"
#include "Buffers/Shaders/ComputeProcessor.hpp"
#include "Buffers/Shaders/DescriptorBindingsProcessor.hpp"
#include "Buffers/Shaders/RenderProcessor.hpp"
#include "Buffers/Shaders/SDFFieldProcessor.hpp"
#include "Buffers/Staging/AudioWriteProcessor.hpp"
#include "Buffers/Staging/BufferDownloadProcessor.hpp"
#include "Buffers/Staging/BufferUploadProcessor.hpp"
#include "Buffers/Staging/DataWriteProcessor.hpp"
#include "Buffers/Textures/NodeTextureBuffer.hpp"
#include "Buffers/Textures/TextureArrayBuffer.hpp"
#include "Buffers/Textures/TextureBuffer.hpp"

#include "Kriya/Awaiters/DelayAwaiters.hpp"
#include "Kriya/Awaiters/EventAwaiter.hpp"
#include "Kriya/Awaiters/NetworkAwaiter.hpp"
#include "Kriya/BroadcastEvents.hpp"
#include "Kriya/BufferPipeline.hpp"
#include "Kriya/Chain.hpp"
#include "Kriya/Chimera.hpp"
#include "Kriya/InputEvents.hpp"
#include "Kriya/NetworkEvents.hpp"
#include "Kriya/SamplingPipeline.hpp"
#include "Kriya/TapSet.hpp"
#include "Kriya/Tasks.hpp"

#include "Vruta/Event.hpp"
#include "Vruta/EventManager.hpp"
#include "Vruta/EventSource.hpp"

#include "Kakshya/Source/CameraContainer.hpp"
#include "Kakshya/Source/DynamicSoundStream.hpp"
#include "Kakshya/Source/SoundFileContainer.hpp"
#include "Kakshya/Source/VideoFileContainer.hpp"
#include "Kakshya/Source/WindowContainer.hpp"

#include "Kinesis/GeometryPrimitives.hpp"

#include "Journal/Archivist.hpp"

#include "Core/Windowing/WindowManager.hpp"

#include "Core/GlobalGraphicsInfo.hpp"
#include "Core/GlobalInputConfig.hpp"
#include "Core/GlobalNetworkConfig.hpp"
#include "Core/GlobalStreamInfo.hpp"

#include "Portal/Graphics/Graphics.hpp"
#include "Portal/Graphics/SamplerForge.hpp"
#include "Portal/Graphics/ShaderFoundry.hpp"
#include "Portal/Graphics/TextureLoom.hpp"

#include "Portal/Network/MessageUtils.hpp"
#include "Portal/Network/Network.hpp"
#include "Portal/Network/NetworkSink.hpp"

#include "Portal/Text/InkPress.hpp"
#include "Portal/Text/Text.hpp"

#include "Portal/Forma/Forma.hpp"
#include "Portal/Forma/Primitives/FormFactory.hpp"

#include "Portal/System/System.hpp"

#include "IO/Audio/SoundFileWriter.hpp"
#include "IO/Camera/CameraSource.hpp"
#include "IO/Camera/FFmpegCameraReader.hpp"
#include "IO/Composite/CompositeReader.hpp"
#include "IO/Composite/CompositeWriter.hpp"
#include "IO/IOManager.hpp"
#include "IO/Image/ImageReader.hpp"
#include "IO/Video/VideoFileWriter.hpp"

#include "Nexus/Tapestry.hpp"

using namespace MayaFlux::Kakshya;
using namespace MayaFlux::Kriya;
using namespace MayaFlux::Buffers;
using namespace MayaFlux::Nodes::Input;
using namespace MayaFlux::Nodes::GpuSync;
using namespace MayaFlux::Nodes::Network;
using namespace MayaFlux::Nodes::Filters;
using namespace MayaFlux::Nodes::Generator;
using namespace MayaFlux::Nodes;
using namespace MayaFlux;
