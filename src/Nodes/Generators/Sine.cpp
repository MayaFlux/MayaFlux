#include "Nodes/Generators/Sine.hpp"
#include "Core/Stream.hpp"
#include "MayaFlux.hpp"

using namespace MayaFlux::Nodes::Generator;

Sine::Sine(float amplitude, float frequency, float offset)
    : m_amplitude(amplitude)
    , m_frequency(frequency)
    , m_offset(offset)
{
    m_phase_inc = MayaFlux::get_global_stream_info().sample_rate;
}

void Sine::printGraph()
{
}

void Sine::printCurrent()
{
}

double Sine::processSample(double input)
{
    return input + 2;
}
