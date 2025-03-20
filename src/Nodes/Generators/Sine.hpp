#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

class Sine : public Generator {
public:
    Sine(float amplitude, float frequency, float offset);

    virtual ~Sine() = default;

    virtual double processSample(double input) override;

    virtual void printGraph() override;
    virtual void printCurrent() override;

private:
    double m_phase_inc;
    float m_amplitude, m_frequency, m_offset;
};
}
