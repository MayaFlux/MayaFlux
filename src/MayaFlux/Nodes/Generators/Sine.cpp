#include "Sine.hpp"
#include "MayaFlux/Core/Stream.hpp"
#include "MayaFlux/MayaFlux.hpp"

using namespace MayaFlux::Nodes::Generator;

Sine::Sine(float amplitude, float frequency, float offset)
    : m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
    , m_phase(0)
{
    m_phase_inc = m_frequency / MayaFlux::get_global_stream_info().sample_rate;
}

void Sine::printGraph()
{
}

void Sine::printCurrent()
{
}

double Sine::processSample(double input)
{
    double current_sample = m_amplitude * std::sin(M_PI_2 * m_phase + m_offset);
    m_phase += m_phase_inc;
    if (input) {
        current_sample += input;
        current_sample *= 0.5f;
    }
    return current_sample;
}
