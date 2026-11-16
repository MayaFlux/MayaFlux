#include "Radiation.hpp"

namespace MayaFlux::Kinesis::Acoustics {

std::optional<RadiationCoefficients> make_radiation_coefficients(
    const RadiationSpec& spec, double end_area, double end_pressure_gain) noexcept
{
    const auto positive = [](double value) { return std::isfinite(value) && value > 0.0; };

    const auto [gamma, lambda, theta] = spec.model;

    if (!positive(gamma) || !positive(lambda) || !positive(theta)
        || !positive(spec.radius) || !positive(spec.sound_speed) || !positive(spec.air_density)
        || !positive(spec.time_step) || !positive(end_area) || !positive(end_pressure_gain)) {
        return std::nullopt;
    }

    const double c = spec.sound_speed;
    const double a = spec.radius;
    const double k = spec.time_step;
    const double impedance = spec.air_density * c / end_area;

    const double inductance = lambda * (a / c) * impedance;
    const double memory_scale = 2.0 * theta * a / (c * k);
    const double denominator = 1.0 + 1.0 / gamma + memory_scale;
    const double branch_weight = 1.0 / denominator;
    const double branch_memory = memory_scale / denominator;

    RadiationCoefficients coefficients;
    coefficients.inductor_gain = k / inductance;
    coefficients.branch_weight = branch_weight;
    coefficients.branch_memory = branch_memory;
    coefficients.memory_conductance = branch_memory / impedance;
    coefficients.conductance = 0.5 * k / inductance + (1.0 - branch_weight) / impedance;
    coefficients.end_half_gain = 0.5 * end_pressure_gain;
    coefficients.reciprocal = 1.0 / (1.0 + coefficients.end_half_gain * coefficients.conductance);
    return coefficients;
}

} // namespace MayaFlux::Kinesis::Acoustics
