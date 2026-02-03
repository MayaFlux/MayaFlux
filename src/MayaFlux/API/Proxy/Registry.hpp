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
        namespace Stochastics {
            class Random;
        }
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
    }
    namespace Network {
        class NodeNetwork;
        class ModalNetwork;
        class ParticleNetwork;
    }
    namespace Input {
        class InputNode;
        class HIDNode;
        class MIDINode;

        struct InputConfig;
        struct HIDConfig;
        struct MIDIConfig;
    }
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

#define ALL_NODE_REGISTRATIONS                                            \
    N(Sine, MayaFlux::Nodes::Generator::Sine)                             \
    N(Phasor, MayaFlux::Nodes::Generator::Phasor)                         \
    N(Impulse, MayaFlux::Nodes::Generator::Impulse)                       \
    N(Logic, MayaFlux::Nodes::Generator::Logic)                           \
    N(Polynomial, MayaFlux::Nodes::Generator::Polynomial)                 \
    N(Random, MayaFlux::Nodes::Generator::Stochastics::Random)            \
    N(IIR, MayaFlux::Nodes::Filters::IIR)                                 \
    N(FIR, MayaFlux::Nodes::Filters::FIR)                                 \
    N(ComputeOutNode, MayaFlux::Nodes::GpuSync::ComputeOutNode)           \
    N(TextureNode, MayaFlux::Nodes::GpuSync::TextureNode)                 \
    N(GeometryWriterNode, MayaFlux::Nodes::GpuSync::GeometryWriterNode)   \
    N(PointNode, MayaFlux::Nodes::GpuSync::PointNode)                     \
    N(PointCollectionNode, MayaFlux::Nodes::GpuSync::PointCollectionNode) \
    N(ProceduralTextureNode, MayaFlux::Nodes::GpuSync::ProceduralTextureNode)

#define ALL_NODE_NETWORK_REGISTRATIONS                      \
    W(ModalNetwork, MayaFlux::Nodes::Network::ModalNetwork) \
    W(ParticleNetwork, MayaFlux::Nodes::Network::ParticleNetwork)

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
