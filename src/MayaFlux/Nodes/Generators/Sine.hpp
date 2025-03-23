#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

class Sine : public Generator {
public:
    Sine(float amplitude = 1, float frequency = 440, float offset = 0, bool bAuto_register = true);

    Sine(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    // Sine(std::shared_ptr<Node> amplitude_modulator, float frequency = 440, float offset = 0, bool bAuto_register = true);

    virtual ~Sine() = default;

    virtual double processSample(double input) override;

    virtual std::vector<double> processFull(unsigned int num_samples) override;

    virtual void printGraph() override;
    virtual void printCurrent() override;

    void register_to_defult();

    void set_frequency(float frequency);

    inline void set_amplitude(float amplitude)
    {
        m_amplitude = amplitude;
    }

    inline void set_params(float frequency, float amplitude, float offset)
    {
        m_amplitude = amplitude;
        m_offset = offset;
        set_frequency(frequency);
    }

private:
    double m_phase_inc, m_phase;
    float m_amplitude, m_frequency, m_offset;
    std::shared_ptr<Node> m_frequency_modulator;
};
}
