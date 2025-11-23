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
    X(Sine, MayaFlux::Nodes::Generator::Sine)                           \
    X(Phasor, MayaFlux::Nodes::Generator::Phasor)                       \
    X(Impulse, MayaFlux::Nodes::Generator::Impulse)                     \
    X(Logic, MayaFlux::Nodes::Generator::Logic)                         \
    X(Polynomial, MayaFlux::Nodes::Generator::Polynomial)               \
    X(Random, MayaFlux::Nodes::Generator::Stochastics::Random)          \
    X(IIR, MayaFlux::Nodes::Filters::IIR)                               \
    X(FIR, MayaFlux::Nodes::Filters::FIR)                               \
    X(ComputeOutNode, MayaFlux::Nodes::GpuSync::ComputeOutNode)         \
    X(TextureNode, MayaFlux::Nodes::GpuSync::TextureNode)               \
    X(GeometryWriterNode, MayaFlux::Nodes::GpuSync::GeometryWriterNode) \
    X(PointNode, MayaFlux::Nodes::GpuSync::PointNode)                   \
    X(PointCollectionNode, MayaFlux::Nodes::GpuSync::PointCollectionNode)

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
