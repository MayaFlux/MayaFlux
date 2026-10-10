#include "Kernels.hpp"

namespace MayaFlux::Kinesis::Discrete {

namespace {

    constexpr double two_pi = 2.0 * std::numbers::pi;

    [[nodiscard]] double interpolate_table(std::span<const double> table, double x) noexcept
    {
        const size_t n = table.size();
        if (n == 0)
            return 0.0;
        if (n == 1)
            return table.front();

        const double clamped = std::clamp(x, -1.0, 1.0);
        const double position = (clamped + 1.0) * 0.5 * static_cast<double>(n - 1);
        const auto index = std::min(static_cast<size_t>(position), n - 2);
        const double fraction = position - static_cast<double>(index);
        return table[index] + (table[index + 1] - table[index]) * fraction;
    }

    inline void wrap_revolution(double& phase) noexcept
    {
        if (phase >= two_pi) {
            phase -= two_pi;
        } else if (phase < 0.0) {
            phase += two_pi;
        }
    }

}

double horner(std::span<const double> coefficients, double x) noexcept
{
    double result = 0.0;
    for (const double c : coefficients)
        result = result * x + c;
    return result;
}

double weighted_sum(std::span<const double> window, std::span<const double> coefs) noexcept
{
    const size_t count = std::min(window.size(), coefs.size());
    double sum = 0.0;
    for (size_t k = 0; k < count; ++k)
        sum += window[k] * coefs[k];
    return sum;
}

double tapped_sum(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (window.empty())
        return 0.0;

    const auto reach = static_cast<double>(window.size() - 1);
    double sum = 0.0;

    for (size_t j = 0; j + 1 < coefs.size(); j += 2) {
        const double lag = coefs[j];
        if (!(lag >= 0.0) || lag > reach)
            continue;

        const auto index = static_cast<size_t>(lag);
        const double fraction = lag - static_cast<double>(index);
        double value = window[index];
        if (fraction > 0.0 && index + 1 < window.size())
            value += (window[index + 1] - value) * fraction;

        sum += coefs[j + 1] * value;
    }
    return sum;
}

double table_lookup(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return interpolate_table(coefs, window.empty() ? 0.0 : window.front());
}

double table_lookup_previous(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return interpolate_table(coefs, window.size() > 1 ? window[1] : 0.0);
}

double chebyshev_series(std::span<const double> window, std::span<const double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const double x = std::clamp(window.empty() ? 0.0 : window.front(), -1.0, 1.0);
    double b1 = 0.0;
    double b2 = 0.0;

    for (size_t k = coefs.size() - 1; k >= 1; --k) {
        const double b0 = coefs[k] + 2.0 * x * b1 - b2;
        b2 = b1;
        b1 = b0;
    }
    return coefs.front() + x * b1 - b2;
}

double iterated_polynomial(std::span<const double> window, std::span<const double> coefs) noexcept
{
    return horner(coefs, window.size() > 1 ? window[1] : 0.0);
}

std::vector<double> phasor_bank_state(
    std::span<const double> increments,
    std::span<const double> scales,
    std::span<const double> am_increments,
    std::span<const double> phases,
    std::span<const double> am_phases)
{
    const size_t n = increments.size();
    std::vector<double> state(1 + 5 * n, 0.0);
    state.front() = static_cast<double>(n);

    for (size_t i = 0; i < n; ++i) {
        state.at(1 + i) = increments[i];
        state.at(1 + n + i) = i < scales.size() ? scales[i] : 1.0;
        state.at(1 + 2 * n + i) = i < am_increments.size() ? am_increments[i] : 0.0;
        state.at(1 + 3 * n + i) = i < phases.size() ? phases[i] : 0.0;
        state.at(1 + 4 * n + i) = i < am_phases.size() ? am_phases[i] : 0.0;
    }
    return state;
}

double phasor_bank(std::span<const double>, std::span<double> coefs) noexcept
{
    if (coefs.empty())
        return 0.0;

    const auto n = static_cast<size_t>(std::max(coefs.front(), 0.0));
    if (n == 0 || coefs.size() < 1 + 5 * n)
        return 0.0;

    const auto increments = coefs.subspan(1, n);
    const auto scales = coefs.subspan(1 + n, n);
    const auto am_increments = coefs.subspan(1 + 2 * n, n);
    const auto phases = coefs.subspan(1 + 3 * n, n);
    const auto am_phases = coefs.subspan(1 + 4 * n, n);

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        phases[i] += increments[i];
        wrap_revolution(phases[i]);

        const double swell = 0.5 + 0.5 * std::sin(am_phases[i]);

        am_phases[i] += am_increments[i];
        wrap_revolution(am_phases[i]);

        sum += scales[i] * swell * std::sin(phases[i]);
    }
    return sum;
}

} // namespace MayaFlux::Kinesis::Discrete
