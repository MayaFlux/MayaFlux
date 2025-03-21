#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes {

namespace Generator {
    class Generator : public Node {
    public:
        virtual ~Generator() = default;
        virtual void printGraph() = 0;
        virtual void printCurrent() = 0;
    };
}
}
