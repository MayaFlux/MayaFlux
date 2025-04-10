#pragma once

#include "Generator.hpp"

namespace MayaFlux::Nodes::Generator {

class Sine : public Generator, public std::enable_shared_from_this<Sine> {
public:
    Sine(float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = false);

    Sine(std::shared_ptr<Node> frequency_modulator, float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator,
        float frequency = 440, float amplitude = 1, float offset = 0, bool bAuto_register = true);

    void Setup(bool bAuto_register);

    virtual ~Sine() = default;

    virtual double process_sample(double input) override;

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

    void set_frequency_modulator(std::shared_ptr<Node> modulator);
    void set_amplitude_modulator(std::shared_ptr<Node> modulator);
    void clear_modulators();

private:
    double m_phase_inc, m_phase;
    float m_amplitude, m_frequency, m_offset;
    std::shared_ptr<Node> m_frequency_modulator;
    std::shared_ptr<Node> m_amplitude_modulator;

    void update_phase_increment(float frequency);
};
}
