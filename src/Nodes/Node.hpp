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

}

}
