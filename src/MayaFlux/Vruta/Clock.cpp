#include "Clock.hpp"
#include "MayaFlux/Utils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Vruta {

SampleClock::SampleClock(uint64_t sample_rate)
    : m_sample_rate(sample_rate)
    , m_current_sample(0)
{
}

void SampleClock::tick(uint64_t samples)
{
    m_current_sample += samples;
}

uint64_t SampleClock::current_position() const
{
    return m_current_sample;
}

double SampleClock::current_time() const
{
    return Utils::units_to_seconds(m_current_sample, m_sample_rate);
}

uint32_t SampleClock::sample_rate() const
{
    return m_sample_rate;
}

uint32_t SampleClock::rate() const
{
    return m_sample_rate;
}

void SampleClock::reset()
{
    m_current_sample = 0;
}

FrameClock::FrameClock(uint32_t target_fps)
    : m_target_fps(target_fps)
    , m_current_frame(0)
    , m_start_time(std::chrono::steady_clock::now())
    , m_last_tick_time(std::chrono::steady_clock::now())
    , m_next_frame_time(std::chrono::steady_clock::now())
    , m_measured_fps(static_cast<double>(target_fps))
{
    recalculate_frame_duration();
}

void FrameClock::tick(uint64_t forced_frames)
{
    auto now = std::chrono::steady_clock::now();

    uint64_t frames_to_advance = forced_frames > 0 ? forced_frames : calculate_elapsed_frames(now);

    if (frames_to_advance > 0) {
        m_current_frame.fetch_add(frames_to_advance, std::memory_order_release);
        update_fps_measurement(now);
        m_last_tick_time = now;
        m_next_frame_time = now + m_frame_duration;
    }
}

uint64_t FrameClock::current_position() const
{
    return m_current_frame.load(std::memory_order_acquire);
}

double FrameClock::current_time() const
{
    return Utils::units_to_seconds(current_position(), m_target_fps);
}

uint32_t FrameClock::rate() const
{
    return m_target_fps;
}

double FrameClock::get_measured_fps() const
{
    return m_measured_fps.load(std::memory_order_acquire);
}

std::chrono::nanoseconds FrameClock::time_until_next_frame() const
{
    auto now = std::chrono::steady_clock::now();
    auto until_next = m_next_frame_time - now;

    if (until_next.count() < 0) {
        return std::chrono::nanoseconds(0);
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(until_next);
}

bool FrameClock::is_frame_late() const
{
    auto now = std::chrono::steady_clock::now();
    return now > m_next_frame_time;
}

uint64_t FrameClock::get_frame_lag() const
{
    auto now = std::chrono::steady_clock::now();

    if (now <= m_next_frame_time) {
        return 0;
    }

    auto elapsed = now - m_next_frame_time;
    auto frames_behind = elapsed / m_frame_duration;
    return static_cast<uint64_t>(frames_behind);
}

void FrameClock::reset()
{
    m_current_frame.store(0, std::memory_order_release);
    m_start_time = std::chrono::steady_clock::now();
    m_last_tick_time = m_start_time;
    m_next_frame_time = m_start_time;
    m_measured_fps.store(static_cast<double>(m_target_fps), std::memory_order_release);
}

void FrameClock::set_target_fps(uint32_t new_fps)
{
    if (new_fps == 0 || new_fps > 1000) {
        error<std::runtime_error>(Journal::Component::Vruta,
            Journal::Context::ClockSync,
            std::source_location::current(),
            "FrameClock", "set_target_fps", "Invalid FPS value: {}",
            new_fps);
    }

    if (new_fps == m_target_fps)
        return;

    m_target_fps = new_fps;
    recalculate_frame_duration();

    auto frames_elapsed = static_cast<double>(m_current_frame.load());
    auto expected_time = m_start_time + std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(frames_elapsed / m_target_fps));

    m_next_frame_time = expected_time + m_frame_duration;
}

void FrameClock::update_fps_measurement(std::chrono::steady_clock::time_point now)
{
    auto dt = std::chrono::duration<double>(now - m_last_tick_time).count();

    if (dt > 0.0 && dt <= 1.0) {
        double instantaneous_fps = 1.0 / dt;

        double current_measured = m_measured_fps.load(std::memory_order_acquire);
        double new_measured = FPS_SMOOTHING_ALPHA * instantaneous_fps + (1.0 - FPS_SMOOTHING_ALPHA) * current_measured;

        m_measured_fps.store(new_measured, std::memory_order_release);
    }
}

uint64_t FrameClock::calculate_elapsed_frames(std::chrono::steady_clock::time_point now) const
{
    auto time_since_last_tick = now - m_last_tick_time;
    return static_cast<uint64_t>(time_since_last_tick / m_frame_duration);
}

void FrameClock::recalculate_frame_duration()
{
    double ns_per_frame = 1'000'000'000.0 / m_target_fps;
    m_frame_duration = std::chrono::nanoseconds(static_cast<uint64_t>(ns_per_frame));
}

void FrameClock::wait_for_next_frame()
{
    auto sleep_duration = time_until_next_frame();
    if (sleep_duration > std::chrono::microseconds(100)) {
        std::this_thread::sleep_until(m_next_frame_time);

    } else if (sleep_duration > std::chrono::nanoseconds(0)) {
        auto wake_time = std::chrono::steady_clock::now() + sleep_duration;

        while (std::chrono::steady_clock::now() < wake_time) {
            std::this_thread::yield();
        }
    }
}

// ========================================
// CustomClock Implementation
// ========================================

CustomClock::CustomClock(uint64_t processing_rate, const std::string& unit_name)
    : m_processing_rate(processing_rate)
    , m_current_position(0)
    , m_unit_name(unit_name)
{
}

void CustomClock::tick(uint64_t units)
{
    m_current_position += units;
}

uint64_t CustomClock::current_position() const
{
    return m_current_position;
}

double CustomClock::current_time() const
{
    return Utils::units_to_seconds(m_current_position, m_processing_rate);
}

uint32_t CustomClock::rate() const
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
