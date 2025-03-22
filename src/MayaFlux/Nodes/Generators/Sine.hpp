#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

class Sine : public Generator {
public:
    Sine(float amplitude = 1, float frequency = 440, float offset = 0);

    virtual ~Sine() = default;

    virtual double processSample(double input) override;

    virtual std::vector<double> processFull(unsigned int num_samples) override;

    virtual void printGraph() override;
    virtual void printCurrent() override;

private:
    double m_phase_inc, m_phase;
    float m_amplitude, m_frequency, m_offset;
};
}
