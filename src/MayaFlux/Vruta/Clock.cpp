#include "Clock.hpp"

namespace MayaFlux::Vruta {

SampleClock::SampleClock(unsigned int sample_rate)
    : m_sample_rate(sample_rate)
    , m_current_sample(0)
{
}

void SampleClock::tick(unsigned int samples)
{
    m_current_sample += samples;
}

unsigned int SampleClock::current_sample() const
{
    return m_current_sample;
}

double SampleClock::current_time() const
{
    return static_cast<double>(m_current_sample) / m_sample_rate;
}

unsigned int SampleClock::sample_rate() const
{
    return m_sample_rate;
}

}
