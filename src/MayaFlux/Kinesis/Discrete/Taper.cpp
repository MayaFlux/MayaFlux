#include "Taper.hpp"

namespace MayaFlux::Kinesis::Discrete {

// ============================================================================
// Coefficient generation
// ============================================================================

std::vector<double> hann(size_t n)
{
    if (n == 0)
        return {};
    if (n == 1)
        return { 1.0 };
    std::vector<double> w(n);
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i)
        w[i] = 0.5 * (1.0 - std::cos(scale * static_cast<double>(i)));
    return w;
}

std::vector<double> hamming(size_t n)
{
    if (n == 0)
        return {};
    if (n == 1)
        return { 1.0 };
    std::vector<double> w(n);
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i)
        w[i] = 0.54 - 0.46 * std::cos(scale * static_cast<double>(i));
    return w;
}

std::vector<double> blackman(size_t n)
{
    if (n == 0)
        return {};
    if (n == 1)
        return { 1.0 };
    std::vector<double> w(n);
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i) {
        const double x = scale * static_cast<double>(i);
        w[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
    }
    return w;
}

std::vector<double> rectangular(size_t n)
{
    return std::vector<double>(n, 1.0);
}

std::vector<double> trapezoid(size_t n, size_t fade_len)
{
    if (n == 0)
        return {};
    if (n == 1)
        return { 1.0 };
    std::vector<double> w(n, 1.0);
    const size_t ramp = std::min(fade_len, n / 2);
    for (size_t i = 0; i < ramp; ++i) {
        const double r = static_cast<double>(i) / static_cast<double>(ramp);
        w[i] = r;
        w[n - 1 - i] = r;
    }
    return w;
}

// ============================================================================
// In-place application
// ============================================================================

void apply_taper(std::span<double> data, std::span<const double> taper) noexcept
{
    if (data.empty() || taper.empty())
        return;
    const size_t n = data.size();
    const size_t tn = taper.size();
    if (n == tn) {
        for (size_t i = 0; i < n; ++i)
            data[i] *= taper[i];
    } else {
        for (size_t i = 0; i < n; ++i)
            data[i] *= taper[i % tn];
    }
}

void apply_hann(std::span<double> data) noexcept
{
    const size_t n = data.size();
    if (n <= 1)
        return;
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i)
        data[i] *= 0.5 * (1.0 - std::cos(scale * static_cast<double>(i)));
}

void apply_hamming(std::span<double> data) noexcept
{
    const size_t n = data.size();
    if (n <= 1)
        return;
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i)
        data[i] *= 0.54 - 0.46 * std::cos(scale * static_cast<double>(i));
}

void apply_blackman(std::span<double> data) noexcept
{
    const size_t n = data.size();
    if (n <= 1)
        return;
    const double scale = std::numbers::pi * 2.0 / static_cast<double>(n - 1);
    for (size_t i = 0; i < n; ++i) {
        const double x = scale * static_cast<double>(i);
        data[i] *= 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
    }
}

void apply_trapezoid(std::span<double> data, size_t fade_len) noexcept
{
    const size_t n = data.size();
    if (n == 0)
        return;
    const size_t ramp = std::min(fade_len, n / 2);
    for (size_t i = 0; i < ramp; ++i) {
        const double r = static_cast<double>(i) / static_cast<double>(ramp);
        data[i] *= r;
        data[n - 1 - i] *= r;
    }
}

} // namespace MayaFlux::Kinesis::Discrete
