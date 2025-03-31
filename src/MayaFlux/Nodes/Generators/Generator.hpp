#pragma once

#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Nodes::Generator {

class Generator : public Node {
public:
    virtual ~Generator() = default;
    virtual void printGraph() = 0;
    virtual void printCurrent() = 0;
};

std::vector<double> HannWindow(size_t length);
std::vector<double> HammingWindow(size_t length);
std::vector<double> BlackmanWindow(size_t length);
std::vector<double> LinearRamp(size_t length, double start = 0.0, double end = 1.0);
std::vector<double> ExponentialRamp(size_t length, double start = 0.001, double end = 1.0);
}
