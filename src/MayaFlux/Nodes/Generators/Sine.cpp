#include "Sine.hpp"
#include "MayaFlux/Core/Stream.hpp"
#include "MayaFlux/MayaFlux.hpp"
#include "MayaFlux/Nodes/NodeGraphManager.hpp"

namespace MayaFlux::Nodes::Generator {

Sine::Sine(float amplitude, float frequency, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
{
    m_phase_inc = (2 * M_PI * m_frequency) / MayaFlux::get_global_stream_info().sample_rate;

    if (bAuto_register) {
        register_to_defult();
    }
}

Sine::Sine(std::shared_ptr<Node> frequency_modulator, float frequency, float amplitude, float offset, bool bAuto_register)
    : m_phase(0)
    , m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_frequency_modulator(frequency_modulator)
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

double Sine::processSample(double input)
{
    if (m_frequency_modulator) {
        double current_freq = m_frequency;
        current_freq += m_frequency_modulator->processSample(0.f);
        m_phase_inc = (2 * M_PI * current_freq) / MayaFlux::get_global_stream_info().sample_rate;
    }

    double current_sample = m_amplitude * std::sin(m_phase + m_offset);
    m_phase += m_phase_inc;
    if (input) {
        current_sample += input;
        current_sample *= 0.5f;
    }

    return current_sample;
}

std::vector<double> Sine::processFull(unsigned int num_samples)
{
    std::vector<double> output;
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
