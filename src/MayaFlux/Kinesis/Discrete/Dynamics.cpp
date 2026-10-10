#include "Dynamics.hpp"

#include "Kernels.hpp"

namespace MayaFlux::Kinesis::Discrete {

double pulse_response(
    double t, double onset, double duration, double rise, double fall, double period) noexcept
{
    double local = t - onset;
    if (local < 0.0)
        return 0.0;

    double pulse = std::max(duration, 0.0);
    if (period > 0.0) {
        local -= period * std::floor(local / period);
        pulse = std::min(pulse, period);
    }

    const auto peak_at = [rise](double s) {
        return rise > 0.0 ? 1.0 - std::exp(-s / rise) : 1.0;
    };

    if (local <= pulse)
        return peak_at(local);

    if (fall <= 0.0)
        return 0.0;
    return peak_at(pulse) * std::exp(-(local - pulse) / fall);
}

void pulse_response(
    std::span<double> out,
    double t,
    std::span<const double> onset,
    std::span<const double> duration,
    std::span<const double> rise,
    std::span<const double> fall,
    std::span<const double> period) noexcept
{
    const size_t count = std::min({ out.size(), onset.size(), duration.size(), rise.size(), fall.size(), period.size() });
    for (size_t i = 0; i < count; ++i)
        out[i] = pulse_response(t, onset[i], duration[i], rise[i], fall[i], period[i]);
}

void lotka_volterra_step(
    std::span<double> levels,
    std::span<const double> growth,
    std::span<const double> interaction,
    double dt)
{
    const size_t n = std::min(levels.size(), growth.size());
    if (n == 0 || interaction.size() < n * n)
        return;

    thread_local std::vector<double> next;
    next.assign(n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        const double pressure = weighted_sum(levels.first(n), interaction.subspan(i * n, n));
        next[i] = std::max(levels[i] + dt * levels[i] * (growth[i] - pressure), 0.0);
    }

    for (size_t i = 0; i < n; ++i)
        levels[i] = next[i];
}

} // namespace MayaFlux::Kinesis::Discrete
