#include "Clock.hpp"
#include "MayaFlux/Utils.hpp"

namespace MayaFlux::Vruta {

SampleClock::SampleClock(u_int64_t sample_rate)
    : m_sample_rate(sample_rate)
    , m_current_sample(0)
{
}

void SampleClock::tick(u_int64_t samples)
{
    m_current_sample += samples;
}

u_int64_t SampleClock::current_position() const
{
    return m_current_sample;
}

double SampleClock::current_time() const
{
    return Utils::units_to_seconds(m_current_sample, m_sample_rate);
}

u_int32_t SampleClock::sample_rate() const
{
    return m_sample_rate;
}

u_int32_t SampleClock::rate() const
{
    return m_sample_rate;
}

void SampleClock::reset()
{
    m_current_sample = 0;
}

FrameClock::FrameClock(u_int64_t frame_rate)
    : m_frame_rate(frame_rate)
    , m_current_frame(0)
{
}

void FrameClock::tick(u_int64_t frames)
{
    m_current_frame += frames;
}

u_int64_t FrameClock::current_position() const
{
    return m_current_frame;
}

u_int64_t FrameClock::current_frame() const
{
    return m_current_frame;
}

double FrameClock::current_time() const
{
    return Utils::units_to_seconds(m_current_frame, m_frame_rate);
}

u_int32_t FrameClock::rate() const
{
    return m_frame_rate;
}

u_int64_t FrameClock::frame_rate() const
{
    return m_frame_rate;
}

void FrameClock::reset()
{
    m_current_frame = 0;
}

// ========================================
// CustomClock Implementation
// ========================================

CustomClock::CustomClock(u_int64_t processing_rate, const std::string& unit_name)
    : m_processing_rate(processing_rate)
    , m_current_position(0)
    , m_unit_name(unit_name)
{
}

void CustomClock::tick(u_int64_t units)
{
    m_current_position += units;
}

u_int64_t CustomClock::current_position() const
{
    return m_current_position;
}

double CustomClock::current_time() const
{
    return Utils::units_to_seconds(m_current_position, m_processing_rate);
}

u_int32_t CustomClock::rate() const
{
    return m_processing_rate;
}

const std::string& CustomClock::unit_name() const
{
    return m_unit_name;
}

void CustomClock::reset()
{
    m_current_position = 0;
}
}
