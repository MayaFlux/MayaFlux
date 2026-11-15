#include "StaggeredLine.hpp"

namespace MayaFlux::Kinesis::Acoustics {

void advance_velocity(
    std::span<double> velocity,
    std::span<const double> pressure,
    std::span<const double> velocity_gain) noexcept
{
    if (pressure.empty()) {
        return;
    }

    const size_t count = std::min({ velocity.size(), velocity_gain.size(), pressure.size() - 1 });
    for (size_t l = 0; l < count; ++l) {
        velocity[l] -= velocity_gain[l] * (pressure[l + 1] - pressure[l]);
    }
}

void advance_pressure(
    std::span<double> pressure,
    std::span<const double> velocity,
    std::span<const double> pressure_gain) noexcept
{
    const size_t end = std::min({ pressure.size(), pressure_gain.size(), velocity.size() + 1 });
    for (size_t l = 1; l + 1 < end; ++l) {
        pressure[l] -= pressure_gain[l] * (velocity[l] - velocity[l - 1]);
    }
}

double discrete_energy(
    std::span<const double> pressure,
    std::span<const double> velocity_previous,
    std::span<const double> velocity_current,
    std::span<const double> pressure_gain,
    std::span<const double> velocity_gain,
    double time_step) noexcept
{
    double potential = 0.0;
    const size_t points = std::min(pressure.size(), pressure_gain.size());
    for (size_t l = 0; l < points; ++l) {
        if (pressure_gain[l] > 0.0) {
            potential += pressure[l] * pressure[l] / pressure_gain[l];
        }
    }

    double kinetic = 0.0;
    const size_t sections = std::min({ velocity_previous.size(), velocity_current.size(), velocity_gain.size() });
    for (size_t l = 0; l < sections; ++l) {
        if (velocity_gain[l] > 0.0) {
            kinetic += velocity_previous[l] * velocity_current[l] / velocity_gain[l];
        }
    }

    return 0.5 * time_step * (potential + kinetic);
}

double sample_pressure(std::span<const double> pressure, double position) noexcept
{
    if (pressure.empty()) {
        return 0.0;
    }
    if (pressure.size() == 1) {
        return pressure.front();
    }

    const double x = std::clamp(position, 0.0, 1.0) * static_cast<double>(pressure.size() - 1);
    const auto index = std::min(static_cast<size_t>(x), pressure.size() - 2);
    const double fraction = x - static_cast<double>(index);
    return pressure[index] + fraction * (pressure[index + 1] - pressure[index]);
}

} // namespace MayaFlux::Kinesis::Acoustics
