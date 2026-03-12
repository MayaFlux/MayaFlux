#include "Transform.hpp"

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Pointwise arithmetic
// ============================================================================

void linear(std::span<double> data, double a, double b) noexcept
{
    std::ranges::transform(data, data.begin(),
        [a, b](double x) { return a * x + b; });
}

void power(std::span<double> data, double exponent) noexcept
{
    std::ranges::transform(data, data.begin(),
        [exponent](double x) { return std::pow(x, exponent); });
}

void exponential(std::span<double> data, double a, double b, double base) noexcept
{
    if (base == std::numbers::e) {
        std::ranges::transform(data, data.begin(),
            [a, b](double x) { return a * std::exp(b * x); });
    } else {
        std::ranges::transform(data, data.begin(),
            [a, b, base](double x) { return a * std::pow(base, b * x); });
    }
}

void logarithmic(std::span<double> data, double a, double b, double c, double base) noexcept
{
    const double log_factor = (base == std::numbers::e) ? 1.0 : (1.0 / std::log(base));
    std::ranges::transform(data, data.begin(),
        [a, b, c, log_factor](double x) {
            const double arg = b * x + c;
            return (arg > 0.0) ? a * std::log(arg) * log_factor : 0.0;
        });
}

void clamp(std::span<double> data, double lo, double hi) noexcept
{
    std::ranges::transform(data, data.begin(),
        [lo, hi](double x) { return std::clamp(x, lo, hi); });
}

void quantize(std::span<double> data, uint8_t bits) noexcept
{
    const double levels = std::pow(2.0, bits) - 1.0;
    const double inv_levels = 1.0 / levels;
    std::ranges::transform(data, data.begin(),
        [levels, inv_levels](double x) {
            const double clamped = std::clamp(x, -1.0, 1.0);
            return static_cast<double>(std::lrint(clamped * levels)) * inv_levels;
        });
}

void normalize(std::span<double> data, double target_min, double target_max) noexcept
{
    if (data.empty())
        return;

    double lo = data[0];
    double hi = data[0];
    for (double v : data) {
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }

    if (hi == lo)
        return;

    const double inv_src = 1.0 / (hi - lo);
    const double dst_range = target_max - target_min;
    std::ranges::transform(data, data.begin(),
        [lo, inv_src, dst_range, target_min](double x) {
            return (x - lo) * inv_src * dst_range + target_min;
        });
}

// ============================================================================
// Temporal shape
// ============================================================================

void reverse(std::span<double> data) noexcept
{
    std::ranges::reverse(data);
}

void fade(std::span<double> data, double fade_in_ratio, double fade_out_ratio) noexcept
{
    if (data.empty())
        return;

    const size_t n = data.size();
    const auto in_end = static_cast<size_t>(fade_in_ratio * static_cast<double>(n));
    const size_t out_start = n - static_cast<size_t>(fade_out_ratio * static_cast<double>(n));

    const double pi = std::numbers::pi;

    for (size_t i = 0; i < in_end; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(in_end);
        data[i] *= std::sin(t * pi * 0.5);
    }

    for (size_t i = out_start; i < n; ++i) {
        const double t = static_cast<double>(i - out_start) / static_cast<double>(n - out_start);
        data[i] *= std::cos(t * pi * 0.5);
    }
}

std::vector<double> slice(std::span<const double> data,
    double start_ratio,
    double end_ratio)
{
    if (data.empty() || end_ratio <= start_ratio)
        return {};

    const size_t s = static_cast<size_t>(std::clamp(start_ratio, 0.0, 1.0) * static_cast<double>(data.size()));
    const size_t e = static_cast<size_t>(std::clamp(end_ratio, 0.0, 1.0) * static_cast<double>(data.size()));

    if (s >= e)
        return {};

    return { data.begin() + s, data.begin() + e };
}

std::vector<double> delay(std::span<const double> data,
    uint32_t delay_samples,
    double fill_value)
{
    std::vector<double> out(data.size() + delay_samples, fill_value);
    std::ranges::copy(data, out.begin() + delay_samples);
    return out;
}

// ============================================================================
// Resampling / interpolation
// ============================================================================

void interpolate_linear(std::span<const double> src, std::span<double> dst) noexcept
{
    const size_t dst_n = dst.size();
    const size_t src_n = src.size();

    if (dst_n == 0 || src_n == 0)
        return;

    if (dst_n == 1 || src_n == 1) {
        std::ranges::fill(dst, src[0]);
        return;
    }

    const double step = static_cast<double>(src_n - 1) / static_cast<double>(dst_n - 1);

    for (size_t i = 0; i < dst_n; ++i) {
        const double pos = static_cast<double>(i) * step;
        const auto idx = static_cast<size_t>(pos);
        const size_t idx1 = std::min(idx + 1, src_n - 1);
        const double frac = pos - static_cast<double>(idx);
        dst[i] = src[idx] + frac * (src[idx1] - src[idx]);
    }
}

void interpolate_cubic(std::span<const double> src, std::span<double> dst) noexcept
{
    const size_t dst_n = dst.size();
    const size_t src_n = src.size();

    if (dst_n == 0 || src_n == 0)
        return;

    if (dst_n == 1 || src_n == 1) {
        std::ranges::fill(dst, src[0]);
        return;
    }

    const double step = static_cast<double>(src_n - 1) / static_cast<double>(dst_n - 1);
    const size_t last = src_n - 1;

    for (size_t i = 0; i < dst_n; ++i) {
        const double pos = static_cast<double>(i) * step;
        const auto idx = static_cast<size_t>(pos);
        const double f = pos - static_cast<double>(idx);

        const size_t i0 = idx > 0 ? idx - 1 : 0;
        const size_t i1 = idx;
        const size_t i2 = std::min(idx + 1, last);
        const size_t i3 = std::min(idx + 2, last);

        const double y0 = src[i0];
        const double y1 = src[i1];
        const double y2 = src[i2];
        const double y3 = src[i3];

        const double a = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
        const double b = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
        const double c = -0.5 * y0 + 0.5 * y2;

        dst[i] = ((a * f + b) * f + c) * f + y1;
    }
}

std::vector<double> time_stretch(std::span<const double> data, double stretch_factor)
{
    if (data.empty() || stretch_factor <= 0.0)
        return {};

    if (stretch_factor == 1.0)
        return { data.begin(), data.end() };

    const auto out_size = static_cast<size_t>(
        static_cast<double>(data.size()) * stretch_factor);

    if (out_size == 0)
        return {};

    std::vector<double> out(out_size);
    interpolate_linear(data, out);
    return out;
}

} // namespace MayaFlux::Kinesis::Discrete
