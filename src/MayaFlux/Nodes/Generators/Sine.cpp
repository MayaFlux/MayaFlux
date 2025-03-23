#include "Sine.hpp"
#include "MayaFlux/Core/Stream.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Nodes::Generator {

Sine::Sine(float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(nullptr)
{
    Setup(bAuto_register);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(nullptr)
{
    Setup(bAuto_register);
}

Sine::Sine(float frequency, std::shared_ptr<Node> amplitude_modulator, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(nullptr)
    , m_amplitude_modulator(amplitude_modulator)
{
    Setup(bAuto_register);
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, std::shared_ptr<Node> amplitude_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
    , m_amplitude_modulator(amplitude_modulator)
{
    Setup(bAuto_register);
}

void Sine::Setup(bool bAuto_register)
{
    m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_global_stream_info().sample_rate;

    if (bAuto_register) {
        register_to_defult();
    }
}

void Sine::set_frequency(float frequency)
{
    m_frequency = frequency;
    m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_global_stream_info().sample_rate;
}

void Sine::update_phase_increment(float frequency)
{
    m_phase_inc = (2 * M_PI * frequency) / MayaFlux::get_global_stream_info().sample_rate;
}

void Sine::set_frequency_modulator(std::shared_ptr<Node> modulator)
{
    m_frequency_modulator = modulator;
}

void Sine::set_amplitude_modulator(std::shared_ptr<Node> modulator)
{
    m_amplitude_modulator = modulator;
}

void Sine::clear_modulators()
{
    m_frequency_modulator = nullptr;
    m_amplitude_modulator = nullptr;
}

double Sine::processSample(double input)
{
    if (m_frequency_modulator) {
        double current_freq = m_frequency;
        current_freq += m_frequency_modulator->processSample(0.f);
        update_phase_increment(current_freq);
    }

    double current_sample = std::sin(m_phase + m_offset);
    m_phase += m_phase_inc;

    if (m_phase > 2 * M_PI) {
        m_phase -= 2 * M_PI;
    } else if (m_phase < -2 * M_PI) {
        m_phase += 2 * M_PI;
    }

    float current_amplitude = m_amplitude;
    if (m_amplitude_modulator) {
        float mod_value = m_amplitude_modulator->processSample(0.f);
        current_amplitude += mod_value;
        m_amplitude = current_amplitude;
    }
    current_sample *= m_amplitude;

    if (input) {
        current_sample += input;
        current_sample *= 0.5f;
    }

    return current_sample;
}

std::vector<double> Sine::processFull(unsigned int num_samples)
{
    std::vector<double> output(num_samples);
    for (unsigned int i = 0; i < num_samples; i++) {
        output[i] = processSample(0.0);
    }
    return output;
}

void Sine::register_to_defult()
{
    MayaFlux::get_node_graph_manager().add_to_root(std::make_shared<Sine>(*this));
}

void Sine::printGraph()
{
}

void Sine::printCurrent()
{
}

}
