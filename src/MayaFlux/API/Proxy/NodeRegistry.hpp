#pragma once

namespace MayaFlux::Nodes {
class Node;
namespace Generator {
    class Sine;
    class Phasor;
    class Impulse;
}
}
// All node registrations in one place
#define ALL_NODE_REGISTRATIONS                    \
    X(sine, MayaFlux::Nodes::Generator::Sine)     \
    X(phasor, MayaFlux::Nodes::Generator::Phasor) \
    X(impulse, MayaFlux::Nodes::Generator::Impulse)\
