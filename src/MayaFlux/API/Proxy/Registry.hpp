#pragma once

namespace MayaFlux {

namespace Nodes {
    class Node;
    namespace Generator {
        class Sine;
        class Phasor;
        class Impulse;
    }
}

namespace Buffers {
    class Buffer;
    class AudioBuffer;
    class NodeBuffer;
    class FeedbackBuffer;
    class ContainerBuffer;
}
}

#define ALL_NODE_REGISTRATIONS                    \
    X(sine, MayaFlux::Nodes::Generator::Sine)     \
    X(phasor, MayaFlux::Nodes::Generator::Phasor) \
    X(impulse, MayaFlux::Nodes::Generator::Impulse)

#define ALL_BUFFER_REGISTRATION                    \
    B(audio, MayaFlux::Buffers::AudioBuffer)       \
    B(node, MayaFlux::Buffers::NodeBuffer)         \
    B(feedback, MayaFlux::Buffers::FeedbackBuffer) \
    B(container, MayaFlux::Buffers::ContainerBuffer)
