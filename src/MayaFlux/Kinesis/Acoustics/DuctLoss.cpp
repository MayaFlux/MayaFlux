#include "DuctLoss.hpp"

namespace MayaFlux::Kinesis::Acoustics {

namespace {

    constexpr double air_viscosity = 1.84e-5;
    constexpr double heat_capacity_ratio = 1.4;
    constexpr double prandtl_number = 0.71;

    [[nodiscard]] bool positive_finite(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

    [[nodiscard]] bool not_negative_finite(double value) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }

}

double viscothermal_attenuation(double radius, double frequency, double sound_speed, double air_density) noexcept
{
    if (!positive_finite(radius) || !positive_finite(frequency)
        || !positive_finite(sound_speed) || !positive_finite(air_density)) {
        return 0.0;
    }

    const double omega = 2.0 * std::numbers::pi * frequency;
    const double thermal = (heat_capacity_ratio - 1.0) / std::sqrt(prandtl_number);
    return std::sqrt(omega * air_viscosity / (2.0 * air_density)) * (1.0 + thermal) / (radius * sound_speed);
}

std::pair<double, double> viscothermal_rates(
    double radius, double sound_speed, double air_density, double low_frequency, double high_frequency) noexcept
{
    if (!positive_finite(low_frequency) || !positive_finite(high_frequency) || !(low_frequency < high_frequency)) {
        return { 0.0, 0.0 };
    }

    const double low = viscothermal_attenuation(radius, low_frequency, sound_speed, air_density);
    const double high = viscothermal_attenuation(radius, high_frequency, sound_speed, air_density);
    if (!(low > 0.0) || !(high > 0.0)) {
        return { 0.0, 0.0 };
    }

    const double slope = (high - low) / (high_frequency * high_frequency - low_frequency * low_frequency);
    const double flat = low - slope * low_frequency * low_frequency;

    const double two_pi = 2.0 * std::numbers::pi;
    const double friction = 2.0 * sound_speed * std::max(flat, 0.0);
    const double viscosity = slope * 2.0 * sound_speed * sound_speed * sound_speed / (two_pi * two_pi);
    return { friction, viscosity };
}

bool DuctLoss::configure(const DuctGrid& grid, double friction, double viscosity)
{
    const size_t n = grid.intervals;
    if (n < 2 || grid.velocity_gain.size() != n || !positive_finite(grid.time_step) || !positive_finite(grid.spacing)
        || !not_negative_finite(friction) || !not_negative_finite(viscosity)) {
        return false;
    }

    const double k = grid.time_step;
    const double diffusion = viscosity * k / (grid.spacing * grid.spacing);

    m_mass.resize(n);
    m_edge.resize(n - 1);
    m_upper.resize(n);
    m_inverse_pivot.resize(n);

    for (size_t l = 0; l < n; ++l) {
        m_mass[l] = k / grid.velocity_gain[l];
    }
    for (size_t l = 0; l + 1 < n; ++l) {
        m_edge[l] = diffusion * 0.5 * (m_mass[l] + m_mass[l + 1]);
    }

    const double ghost_left = 2.0 * m_edge.front();
    const double ghost_right = 2.0 * m_edge.back();

    double upper_before = 0.0;
    for (size_t l = 0; l < n; ++l) {
        const double left = l > 0 ? m_edge[l - 1] : ghost_left;
        const double right = l + 1 < n ? m_edge[l] : ghost_right;
        const double diagonal = m_mass[l] * (1.0 + k * friction) + left + right;

        const double pivot = diagonal - (l > 0 ? left * upper_before : 0.0);
        m_inverse_pivot[l] = 1.0 / pivot;
        m_upper[l] = l + 1 < n ? m_edge[l] * m_inverse_pivot[l] : 0.0;
        upper_before = m_upper[l];
    }

    m_friction = friction;
    m_viscosity = viscosity;
    m_active = friction > 0.0 || viscosity > 0.0;
    return true;
}

void DuctLoss::apply(std::span<double> velocity) const noexcept
{
    const size_t n = m_mass.size();
    if (!m_active || n == 0 || velocity.size() != n) {
        return;
    }

    velocity[0] = m_mass[0] * velocity[0] * m_inverse_pivot[0];
    for (size_t l = 1; l < n; ++l) {
        velocity[l] = (m_mass[l] * velocity[l] + m_edge[l - 1] * velocity[l - 1]) * m_inverse_pivot[l];
    }
    for (size_t l = n - 1; l > 0; --l) {
        velocity[l - 1] += m_upper[l - 1] * velocity[l];
    }
}

} // namespace MayaFlux::Kinesis::Acoustics
