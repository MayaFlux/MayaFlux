#include "TimeSampling.hpp"

#include "MayaFlux/Kinesis/Scalar.hpp"

namespace MayaFlux::Kinesis {

namespace {

    constexpr int64_t k_exact_limit = int64_t { 1 } << 53;

    uint32_t greatest_common_divisor(uint32_t a, uint32_t b) noexcept
    {
        while (b != 0) {
            const uint32_t next = a % b;
            a = b;
            b = next;
        }
        return a;
    }

    bool all_finite(const std::vector<double>& values) noexcept
    {
        return std::ranges::all_of(values, [](double v) { return std::isfinite(v); });
    }

    bool strictly_increasing(const std::vector<double>& values) noexcept
    {
        return std::ranges::adjacent_find(values, std::greater_equal<> {}) == values.end();
    }

    double saturate_cycles(double cycles) noexcept
    {
        return std::clamp(cycles, -9.0e18, 9.0e18);
    }

}

std::optional<FoldedTime> fold_time(double t, double begin, double end, TimeWrap mode)
{
    if (!std::isfinite(t) || !std::isfinite(begin) || !std::isfinite(end) || end < begin) {
        return std::nullopt;
    }

    const double range = end - begin;
    if (range <= 0.0) {
        return FoldedTime { .time = begin, .cycles = 0, .reversed = false };
    }

    if (mode == TimeWrap::Clamp) {
        return FoldedTime { .time = std::clamp(t, begin, end), .cycles = 0, .reversed = false };
    }

    const double cycles = std::floor((t - begin) / range);
    const auto whole = static_cast<int64_t>(saturate_cycles(cycles));

    if (mode == TimeWrap::Repeat) {
        return FoldedTime {
            .time = std::clamp(wrap(t, begin, end), begin, end),
            .cycles = whole,
            .reversed = false
        };
    }

    return FoldedTime {
        .time = std::clamp(ping_pong(t, begin, end), begin, end),
        .cycles = whole,
        .reversed = (whole & 1) != 0
    };
}

std::optional<TimeSampling> TimeSampling::uniform(double step, double start)
{
    if (!std::isfinite(step) || !std::isfinite(start) || !(step > 0.0) || !(start + step > start)) {
        return std::nullopt;
    }

    TimeSampling out;
    out.m_kind = Kind::Uniform;
    out.m_step = step;
    out.m_start = start;
    return out;
}

std::optional<TimeSampling> TimeSampling::uniform_rate(RationalRate rate, int64_t first_frame)
{
    if (rate.numerator == 0 || rate.denominator == 0) {
        return std::nullopt;
    }

    const uint32_t divisor = greatest_common_divisor(rate.numerator, rate.denominator);
    rate.numerator /= divisor;
    rate.denominator /= divisor;

    TimeSampling out;
    out.m_kind = Kind::Uniform;
    out.m_rate = rate;
    out.m_first_frame = first_frame;
    out.m_step = static_cast<double>(rate.denominator) / static_cast<double>(rate.numerator);
    out.m_start = static_cast<double>(first_frame) * out.m_step;
    return out;
}

std::optional<TimeSampling> TimeSampling::cyclic(std::vector<double> times, double time_per_cycle)
{
    if (times.size() < 2 || times.size() > std::numeric_limits<uint32_t>::max()
        || !std::isfinite(time_per_cycle) || !(time_per_cycle > 0.0)
        || !all_finite(times) || !strictly_increasing(times)
        || !(times.back() < times.front() + time_per_cycle)) {
        return std::nullopt;
    }

    TimeSampling out;
    out.m_kind = Kind::Cyclic;
    out.m_times = std::move(times);
    out.m_period = time_per_cycle;
    return out;
}

std::optional<TimeSampling> TimeSampling::acyclic(std::vector<double> times)
{
    if (times.empty() || !all_finite(times) || !strictly_increasing(times)) {
        return std::nullopt;
    }

    TimeSampling out;
    out.m_kind = Kind::Acyclic;
    out.m_times = std::move(times);
    return out;
}

std::optional<TimeSampling> TimeSampling::from_cycles(
    uint32_t samples_per_cycle, double time_per_cycle, std::vector<double> times)
{
    if (samples_per_cycle == 0 || times.size() != samples_per_cycle) {
        return std::nullopt;
    }
    if (samples_per_cycle == 1) {
        return uniform(time_per_cycle, times.front());
    }
    return cyclic(std::move(times), time_per_cycle);
}

std::optional<size_t> TimeSampling::stored_count() const noexcept
{
    if (m_kind != Kind::Acyclic) {
        return std::nullopt;
    }
    return m_times.size();
}

std::optional<double> TimeSampling::rational_time(size_t index) const noexcept
{
    constexpr auto k_max = std::numeric_limits<int64_t>::max();

    if (index > static_cast<size_t>(k_max)) {
        return std::nullopt;
    }
    const auto offset = static_cast<int64_t>(index);
    if (m_first_frame > k_max - offset) {
        return std::nullopt;
    }
    const int64_t frame = m_first_frame + offset;

    const auto denominator = static_cast<int64_t>(m_rate->denominator);
    const auto numerator = static_cast<int64_t>(m_rate->numerator);

    if (frame > 0 ? frame > k_max / denominator : frame < std::numeric_limits<int64_t>::min() / denominator) {
        return std::nullopt;
    }
    const int64_t product = frame * denominator;

    if (product >= -k_exact_limit && product <= k_exact_limit) {
        return static_cast<double>(product) / static_cast<double>(numerator);
    }

    const int64_t whole = product / numerator;
    const int64_t rest = product % numerator;
    return static_cast<double>(whole) + static_cast<double>(rest) / static_cast<double>(numerator);
}

std::optional<double> TimeSampling::time(size_t index) const noexcept
{
    switch (m_kind) {
    case Kind::Uniform:
        if (m_rate) {
            return rational_time(index);
        }
        return m_start + static_cast<double>(index) * m_step;
    case Kind::Cyclic: {
        const size_t per_cycle = m_times.size();
        const size_t cycle = index / per_cycle;
        return static_cast<double>(cycle) * m_period + m_times[index % per_cycle];
    }
    case Kind::Acyclic:
        if (index < m_times.size()) {
            return m_times[index];
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<TimeLocation> TimeSampling::locate(double t, size_t count) const noexcept
{
    if (count == 0 || !std::isfinite(t)) {
        return std::nullopt;
    }
    if (m_kind == Kind::Acyclic && count > m_times.size()) {
        return std::nullopt;
    }

    const auto first = time(0);
    const auto last = time(count - 1);
    if (!first || !last) {
        return std::nullopt;
    }

    if (t < *first) {
        return TimeLocation { .lower = 0, .upper = 0, .fraction = 0.0, .placement = TimePlacement::Before };
    }
    if (t > *last) {
        return TimeLocation { .lower = count - 1, .upper = count - 1, .fraction = 0.0, .placement = TimePlacement::After };
    }

    const auto at_or_below = [&](size_t index) {
        const auto sample = time(index);
        return sample && *sample <= t;
    };

    size_t low = 0;
    size_t high = count - 1;
    while (low < high) {
        const size_t middle = low + (high - low + 1) / 2;
        if (at_or_below(middle)) {
            low = middle;
        } else {
            high = middle - 1;
        }
    }

    const double lower_time = *time(low);
    if (low + 1 >= count || lower_time == t) {
        return TimeLocation { .lower = low, .upper = low, .fraction = 0.0, .placement = TimePlacement::Within };
    }

    const double upper_time = *time(low + 1);
    return TimeLocation {
        .lower = low,
        .upper = low + 1,
        .fraction = (t - lower_time) / (upper_time - lower_time),
        .placement = TimePlacement::Within
    };
}

std::optional<size_t> TimeSampling::floor_index(double t, size_t count) const noexcept
{
    const auto where = locate(t, count);
    if (!where) {
        return std::nullopt;
    }
    return where->lower;
}

std::optional<size_t> TimeSampling::ceil_index(double t, size_t count) const noexcept
{
    const auto where = locate(t, count);
    if (!where) {
        return std::nullopt;
    }
    if (where->placement == TimePlacement::Within && where->lower != where->upper) {
        return where->upper;
    }
    return where->lower;
}

std::optional<size_t> TimeSampling::nearest_index(double t, size_t count) const noexcept
{
    const auto where = locate(t, count);
    if (!where) {
        return std::nullopt;
    }
    if (where->lower == where->upper) {
        return where->lower;
    }
    const double lower_time = *time(where->lower);
    const double upper_time = *time(where->upper);
    return (t - lower_time) <= (upper_time - t) ? where->lower : where->upper;
}

}
