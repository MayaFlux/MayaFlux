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
}

namespace Buffers {
    class Buffer;
    class AudioBuffer;
    class NodeBuffer;
    class FeedbackBuffer;
    class ContainerBuffer;
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
    X(FIR, MayaFlux::Nodes::Filters::FIR)

#define ALL_BUFFER_REGISTRATION                          \
    B(AudioBuffer, MayaFlux::Buffers::AudioBuffer)       \
    B(NodeBuffer, MayaFlux::Buffers::NodeBuffer)         \
    B(FeedbackBuffer, MayaFlux::Buffers::FeedbackBuffer) \
    B(ContainerBuffer, MayaFlux::Buffers::ContainerBuffer)
