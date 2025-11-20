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
    class ComputeOutNode;
    class TextureNode;
    class GeometryWriterNode;
}

namespace Buffers {
    class Buffer;
    class AudioBuffer;
    class NodeBuffer;
    class FeedbackBuffer;
    class ContainerBuffer;
    class VKBuffer;
    class TextureBuffer;
    class GeometryBuffer;
}

namespace Kakshya {
    class SignalSourceContainer;
    class DynamicSoundStream;
    class SoundFileContainer;
}
}

#define ALL_NODE_REGISTRATIONS                                 \
    X(Sine, MayaFlux::Nodes::Generator::Sine)                  \
    X(Phasor, MayaFlux::Nodes::Generator::Phasor)              \
    X(Impulse, MayaFlux::Nodes::Generator::Impulse)            \
    X(Logic, MayaFlux::Nodes::Generator::Logic)                \
    X(Polynomial, MayaFlux::Nodes::Generator::Polynomial)      \
    X(Random, MayaFlux::Nodes::Generator::Stochastics::Random) \
    X(IIR, MayaFlux::Nodes::Filters::IIR)                      \
    X(FIR, MayaFlux::Nodes::Filters::FIR)                      \
    X(ComputeOutNode, MayaFlux::Nodes::ComputeOutNode)         \
    X(TextureNode, MayaFlux::Nodes::TextureNode)               \
    X(GeometryWriterNode, MayaFlux::Nodes::GeometryWriterNode)

#define ALL_BUFFER_REGISTRATION                            \
    B(AudioBuffer, MayaFlux::Buffers::AudioBuffer)         \
    B(NodeBuffer, MayaFlux::Buffers::NodeBuffer)           \
    B(FeedbackBuffer, MayaFlux::Buffers::FeedbackBuffer)   \
    B(ContainerBuffer, MayaFlux::Buffers::ContainerBuffer) \
    B(VKBuffer, MayaFlux::Buffers::VKBuffer)               \
    B(TextureBuffer, MayaFlux::Buffers::TextureBuffer)     \
    B(GeometryBuffer, MayaFlux::Buffers::GeometryBuffer)
