#pragma once

namespace MayaFlux {

namespace Nodes {
    class Node;
    namespace Generator {
        class Sine;
        class Phasor;
        class Impulse;
        class Logic;
        class Polynomial;
        class Random;
    }
    namespace Filters {
        class IIR;
        class FIR;
    }
    namespace GpuSync {
        class ComputeOutNode;
        class TextureNode;
        class GeometryWriterNode;
        class PointNode;
        class PointCollectionNode;
        class ProceduralTextureNode;
        class PathGeneratorNode;
        class TopologyGeneratorNode;
    }
    namespace Network {
        class NodeNetwork;
        class ModalNetwork;
        class WaveguideNetwork;
        class ParticleNetwork;
        class PointCloudNetwork;
    }
    namespace Input {
        class InputNode;
        class HIDNode;
        class MIDINode;

        struct InputConfig;
        struct HIDConfig;
        struct MIDIConfig;
    }
    class Constant;
}

namespace Buffers {
    class Buffer;
    class AudioBuffer;
    class NodeBuffer;
    class FeedbackBuffer;
    class SoundContainerBuffer;
    class VKBuffer;
    class NodeTextureBuffer;
    class TextureBuffer;
    class GeometryBuffer;
    class NetworkGeometryBuffer;
}

namespace Kakshya {
    class SignalSourceContainer;
    class DynamicSoundStream;
    class SoundFileContainer;
}

namespace Core {
    struct InputBinding;
}
}

#define ALL_NODE_REGISTRATIONS                                                \
    N(Sine, MayaFlux::Nodes::Generator::Sine)                                 \
    N(Phasor, MayaFlux::Nodes::Generator::Phasor)                             \
    N(Impulse, MayaFlux::Nodes::Generator::Impulse)                           \
    N(Logic, MayaFlux::Nodes::Generator::Logic)                               \
    N(Polynomial, MayaFlux::Nodes::Generator::Polynomial)                     \
    N(Random, MayaFlux::Nodes::Generator::Random)                             \
    N(IIR, MayaFlux::Nodes::Filters::IIR)                                     \
    N(FIR, MayaFlux::Nodes::Filters::FIR)                                     \
    N(ComputeOutNode, MayaFlux::Nodes::GpuSync::ComputeOutNode)               \
    N(TextureNode, MayaFlux::Nodes::GpuSync::TextureNode)                     \
    N(GeometryWriterNode, MayaFlux::Nodes::GpuSync::GeometryWriterNode)       \
    N(PointNode, MayaFlux::Nodes::GpuSync::PointNode)                         \
    N(PathGeneratorNode, MayaFlux::Nodes::GpuSync::PathGeneratorNode)         \
    N(PointCollectionNode, MayaFlux::Nodes::GpuSync::PointCollectionNode)     \
    N(TopologyGeneratorNode, MayaFlux::Nodes::GpuSync::TopologyGeneratorNode) \
    N(ProceduralTextureNode, MayaFlux::Nodes::GpuSync::ProceduralTextureNode) \
    N(Constant, MayaFlux::Nodes::Constant)

#define ALL_NODE_NETWORK_REGISTRATIONS                              \
    W(ModalNetwork, MayaFlux::Nodes::Network::ModalNetwork)         \
    W(WaveguideNetwork, MayaFlux::Nodes::Network::WaveguideNetwork) \
    W(ParticleNetwork, MayaFlux::Nodes::Network::ParticleNetwork)   \
    W(PointCloudNetwork, MayaFlux::Nodes::Network::PointCloudNetwork)

#define ALL_BUFFER_REGISTRATION                                      \
    B(AudioBuffer, MayaFlux::Buffers::AudioBuffer)                   \
    B(NodeBuffer, MayaFlux::Buffers::NodeBuffer)                     \
    B(FeedbackBuffer, MayaFlux::Buffers::FeedbackBuffer)             \
    B(SoundContainerBuffer, MayaFlux::Buffers::SoundContainerBuffer) \
    B(VKBuffer, MayaFlux::Buffers::VKBuffer)                         \
    B(NodeTextureBuffer, MayaFlux::Buffers::NodeTextureBuffer)       \
    B(TextureBuffer, MayaFlux::Buffers::TextureBuffer)               \
    B(GeometryBuffer, MayaFlux::Buffers::GeometryBuffer)             \
    B(NetworkGeometryBuffer, MayaFlux::Buffers::NetworkGeometryBuffer)
