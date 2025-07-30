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
            class NoiseEngine;
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

#define ALL_NODE_REGISTRATIONS                                          \
    X(sine, MayaFlux::Nodes::Generator::Sine)                           \
    X(phasor, MayaFlux::Nodes::Generator::Phasor)                       \
    X(impulse, MayaFlux::Nodes::Generator::Impulse)                     \
    X(logic, MayaFlux::Nodes::Generator::Logic)                         \
    X(polynomial, MayaFlux::Nodes::Generator::Polynomial)               \
    X(stochastic, MayaFlux::Nodes::Generator::Stochastics::NoiseEngine) \
    X(iir, MayaFlux::Nodes::Filters::IIR)                               \
    X(fir, MayaFlux::Nodes::Filters::FIR)

#define ALL_BUFFER_REGISTRATION                    \
    B(audio, MayaFlux::Buffers::AudioBuffer)       \
    B(node, MayaFlux::Buffers::NodeBuffer)         \
    B(feedback, MayaFlux::Buffers::FeedbackBuffer) \
    B(container, MayaFlux::Buffers::ContainerBuffer)
