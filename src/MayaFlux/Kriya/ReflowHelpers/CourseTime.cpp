#include "CourseTime.hpp"

#include "MayaFlux/Kinesis/Temporal/TimeSampling.hpp"

namespace MayaFlux::Kriya {

namespace {

    bool usable(const CourseSetting& setting)
    {
        return setting.usable();
    }

    bool has(const std::shared_ptr<const Kinesis::TimeMap>& map)
    {
        return map && map->fn;
    }

    uint64_t distance(int64_t a, int64_t b)
    {
        return a >= b
            ? static_cast<uint64_t>(a) - static_cast<uint64_t>(b)
            : static_cast<uint64_t>(b) - static_cast<uint64_t>(a);
    }

    std::optional<double> unfolded(const CourseSetting& setting, double clock_offset, double anchor)
    {
        const double u = has(setting.warp) ? (*setting.warp)(clock_offset) : anchor + clock_offset;
        return std::isfinite(u) ? std::optional<double> { u } : std::nullopt;
    }

    double default_anchor(const CourseSetting& setting)
    {
        return setting.rate >= 0.0 ? setting.in : setting.out;
    }

    Kinesis::TimeWrap wrap_of(CourseEdge edge)
    {
        switch (edge) {
        case CourseEdge::Loop:
            return Kinesis::TimeWrap::Repeat;
        case CourseEdge::PingPong:
            return Kinesis::TimeWrap::PingPong;
        default:
            return Kinesis::TimeWrap::Clamp;
        }
    }

}

std::optional<CourseTime> CourseTime::create(double step_rate)
{
    if (!std::isfinite(step_rate) || !(step_rate > 0.0)) {
        return std::nullopt;
    }
    const double period = 1.0 / step_rate;
    if (!std::isfinite(period) || !(period > 0.0)) {
        return std::nullopt;
    }
    return CourseTime(period);
}

double CourseTime::clock_at(uint64_t step) const noexcept
{
    return m_base_clock + m_rate * static_cast<double>(step - m_base_step) * m_period;
}

std::optional<CourseStep> CourseTime::step(const CourseSetting& setting)
{
    if (!usable(setting)) {
        return std::nullopt;
    }
    if (m_finished) {
        return m_last;
    }

    const uint64_t next_step = m_steps + 1;
    const double previous_clock = clock_at(m_steps);

    uint64_t base_step = m_base_step;
    double base_clock = m_base_clock;
    if (setting.rate != m_rate) {
        base_step = m_steps;
        base_clock = previous_clock;
    }

    const double clock = base_clock + setting.rate * static_cast<double>(next_step - base_step) * m_period;
    if (!std::isfinite(clock)) {
        return std::nullopt;
    }

    const bool repeating = has(setting.repeat);
    double origin = 0.0;
    std::optional<double> length;
    bool restarted = false;
    if (repeating) {
        const auto length_at = [&](double at) {
            const double seconds = (*setting.repeat)(at);
            return std::isfinite(seconds) ? std::max(seconds, m_period) : m_period;
        };

        origin = m_repeat_length ? m_repeat_origin : previous_clock;
        length = m_repeat_length ? m_repeat_length : std::optional<double> { length_at(origin) };
        if (std::abs(clock - origin) >= *length) {
            origin = clock;
            length = length_at(origin);
            restarted = true;
        }
    }

    const double anchor = m_anchor.value_or(default_anchor(setting));
    const auto now = unfolded(setting, (clock - origin) + setting.offset, anchor);
    if (!now) {
        return std::nullopt;
    }
    std::optional<double> before = now;
    if (!restarted) {
        if (const auto prior = unfolded(setting, (previous_clock - origin) + setting.offset, anchor)) {
            before = prior;
        }
    }

    CourseStep result;
    uint64_t total = m_crossings;
    bool finished = false;

    switch (setting.edge) {
    case CourseEdge::Once:
        finished = *now < setting.in || *now >= setting.out;
        result.time = std::clamp(*now, setting.in, setting.out);
        break;
    case CourseEdge::Hold:
        result.time = std::clamp(*now, setting.in, setting.out);
        break;
    case CourseEdge::Loop:
    case CourseEdge::PingPong: {
        const auto mode = wrap_of(setting.edge);
        const auto at = Kinesis::fold_time(*now, setting.in, setting.out, mode);
        const auto was = Kinesis::fold_time(*before, setting.in, setting.out, mode);
        if (!at || !was) {
            return std::nullopt;
        }

        result.time = at->time;
        result.reversed = at->reversed;
        result.crossings = distance(at->cycles, was->cycles);
        total = result.crossings > std::numeric_limits<uint64_t>::max() - total
            ? std::numeric_limits<uint64_t>::max()
            : total + result.crossings;

        if (setting.edge_count > 0 && total >= setting.edge_count) {
            finished = true;
            if (result.crossings > 0) {
                result.time = at->cycles >= was->cycles ? setting.out : setting.in;
            }
        }
        break;
    }
    }
    result.finished = finished;
    const uint64_t passes = m_passes + (restarted ? 1U : 0U);
    result.passes = passes;

    m_steps = next_step;
    m_base_step = base_step;
    m_base_clock = base_clock;
    m_rate = setting.rate;
    if (!m_anchor) {
        m_anchor = anchor;
    }
    if (repeating) {
        m_repeat_origin = origin;
        m_repeat_length = length;
    } else {
        m_repeat_origin = 0.0;
        m_repeat_length.reset();
    }
    m_crossings = total;
    m_passes = passes;
    m_finished = finished;
    m_last = result;
    return result;
}

bool CourseTime::seek(double source_time, const CourseSetting& setting)
{
    if (!usable(setting) || has(setting.warp) || !std::isfinite(source_time)) {
        return false;
    }
    restart();
    m_anchor = source_time - setting.offset;
    return true;
}

bool CourseTime::seek_clock(double clock_seconds)
{
    if (!std::isfinite(clock_seconds)) {
        return false;
    }
    m_base_step = m_steps;
    m_base_clock = clock_seconds;
    m_repeat_origin = 0.0;
    m_repeat_length.reset();
    m_crossings = 0;
    m_passes = 0;
    m_finished = false;
    return true;
}

void CourseTime::restart()
{
    m_base_step = m_steps;
    m_base_clock = 0.0;
    m_anchor.reset();
    m_repeat_origin = 0.0;
    m_repeat_length.reset();
    m_crossings = 0;
    m_passes = 0;
    m_finished = false;
}

std::optional<double> CourseTime::resolve(const CourseSetting& setting, double extra_offset) const
{
    if (!usable(setting) || !std::isfinite(extra_offset)) {
        return std::nullopt;
    }
    if (m_finished) {
        return m_last.time;
    }

    const double now = clock_at(m_steps);
    double origin = 0.0;
    if (has(setting.repeat)) {
        origin = m_repeat_length ? m_repeat_origin : now;
    }

    const double anchor = m_anchor.value_or(default_anchor(setting));
    const auto u = unfolded(setting, (now - origin) + setting.offset + extra_offset, anchor);
    if (!u) {
        return std::nullopt;
    }

    const auto folded = Kinesis::fold_time(*u, setting.in, setting.out, wrap_of(setting.edge));
    return folded ? std::optional<double> { folded->time } : std::nullopt;
}

} // namespace MayaFlux::Kriya
