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
    }
    namespace Network {

        class NodeNetwork;
        class ModalNetwork;
        class ParticleNetwork;
    }
}

namespace Buffers {
    class Buffer;
    class AudioBuffer;
    class NodeBuffer;
    class FeedbackBuffer;
    class ContainerBuffer;
    class VKBuffer;
    class TextureBindBuffer;
    class TextureBuffer;
    class GeometryBuffer;
    class NetworkGeometryBuffer;
}

namespace Kakshya {
    class SignalSourceContainer;
    class DynamicSoundStream;
    class SoundFileContainer;
}
}

#define ALL_NODE_REGISTRATIONS                                          \
    N(Sine, MayaFlux::Nodes::Generator::Sine)                           \
    N(Phasor, MayaFlux::Nodes::Generator::Phasor)                       \
    N(Impulse, MayaFlux::Nodes::Generator::Impulse)                     \
    N(Logic, MayaFlux::Nodes::Generator::Logic)                         \
    N(Polynomial, MayaFlux::Nodes::Generator::Polynomial)               \
    N(Random, MayaFlux::Nodes::Generator::Stochastics::Random)          \
    N(IIR, MayaFlux::Nodes::Filters::IIR)                               \
    N(FIR, MayaFlux::Nodes::Filters::FIR)                               \
    N(ComputeOutNode, MayaFlux::Nodes::GpuSync::ComputeOutNode)         \
    N(TextureNode, MayaFlux::Nodes::GpuSync::TextureNode)               \
    N(GeometryWriterNode, MayaFlux::Nodes::GpuSync::GeometryWriterNode) \
    N(PointNode, MayaFlux::Nodes::GpuSync::PointNode)                   \
    N(PointCollectionNode, MayaFlux::Nodes::GpuSync::PointCollectionNode)

#define ALL_NODE_NETWORK_REGISTRATIONS                      \
    W(ModalNetwork, MayaFlux::Nodes::Network::ModalNetwork) \
    W(ParticleNetwork, MayaFlux::Nodes::Network::ParticleNetwork)

#define ALL_BUFFER_REGISTRATION                                \
    B(AudioBuffer, MayaFlux::Buffers::AudioBuffer)             \
    B(NodeBuffer, MayaFlux::Buffers::NodeBuffer)               \
    B(FeedbackBuffer, MayaFlux::Buffers::FeedbackBuffer)       \
    B(ContainerBuffer, MayaFlux::Buffers::ContainerBuffer)     \
    B(VKBuffer, MayaFlux::Buffers::VKBuffer)                   \
    B(TextureBindBuffer, MayaFlux::Buffers::TextureBindBuffer) \
    B(TextureBuffer, MayaFlux::Buffers::TextureBuffer)         \
    B(GeometryBuffer, MayaFlux::Buffers::GeometryBuffer)       \
    B(NetworkGeometryBuffer, MayaFlux::Buffers::NetworkGeometryBuffer)
