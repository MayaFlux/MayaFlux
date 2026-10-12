#include "Series.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    [[nodiscard]] double free_bar_root(size_t order) noexcept
    {
        double x = (2.0 * static_cast<double>(order) + 1.0) * std::numbers::pi * 0.5;
        for (int iteration = 0; iteration < 50; ++iteration) {
            const double sech = 1.0 / std::cosh(x);
            const double value = std::cos(x) - sech;
            const double slope = -std::sin(x) + sech * std::tanh(x);
            const double step = value / slope;
            x -= step;
            if (std::abs(step) < 1e-15 * std::abs(x))
                break;
        }
        return x;
    }

}

std::vector<double> harmonic_series(size_t count, double base)
{
    std::vector<double> terms(count);
    for (size_t i = 0; i < count; ++i)
        terms[i] = base * static_cast<double>(i + 1);
    return terms;
}

std::vector<double> stretched_series(size_t count, double stretch, double base)
{
    std::vector<double> terms(count);
    for (size_t i = 0; i < count; ++i) {
        const auto n = static_cast<double>(i + 1);
        terms[i] = base * n * std::sqrt(1.0 + stretch * n * n);
    }
    return terms;
}

std::vector<double> free_bar_series(size_t count, double base)
{
    std::vector<double> terms(count);
    if (count == 0)
        return terms;

    const double first = free_bar_root(1);
    for (size_t i = 0; i < count; ++i) {
        const double ratio = free_bar_root(i + 1) / first;
        terms[i] = base * ratio * ratio;
    }
    return terms;
}

std::vector<double> power_series(size_t count, double ratio, double step, double base)
{
    std::vector<double> terms(count);
    for (size_t i = 0; i < count; ++i)
        terms[i] = base * std::pow(ratio, static_cast<double>(i + 1) * step);
    return terms;
}

std::vector<double> split_pairs(std::span<const double> values, double spread)
{
    std::vector<double> pairs;
    pairs.reserve(values.size() * 2);
    for (const double v : values) {
        pairs.push_back(v * (1.0 - spread));
        pairs.push_back(v * (1.0 + spread));
    }
    return pairs;
}

} // namespace MayaFlux::Kinesis::Discrete
