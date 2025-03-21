#pragma once

#include "config.h"

namespace MayaFlux {

namespace Nodes {

    class Node {

    public:
        virtual ~Node() = default;
        virtual double processSample(double input) = 0;
        virtual std::vector<double> processFull(unsigned int num_samples) = 0;
    };

    std::shared_ptr<Node> operator>>(std::shared_ptr<Node> lhs, std::shared_ptr<Node> rhs);
}

}
