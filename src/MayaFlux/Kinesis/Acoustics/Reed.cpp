#include "Reed.hpp"

namespace MayaFlux::Kinesis::Acoustics {

namespace {

    [[nodiscard]] bool positive_finite(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

    [[nodiscard]] bool channel_valid(const ReedChannel& reed) noexcept
    {
        return positive_finite(reed.width) && positive_finite(reed.rest_opening)
            && positive_finite(reed.closing_pressure) && positive_finite(reed.air_density);
    }

}

double reed_flow_coefficient(const ReedChannel& reed) noexcept
{
    return reed.width * reed.rest_opening * std::sqrt(2.0 / reed.air_density);
}

double reed_flow(const ReedChannel& reed, double difference) noexcept
{
    if (!channel_valid(reed)) {
        return 0.0;
    }

    const double opening = 1.0 - difference / reed.closing_pressure;
    if (opening <= 0.0) {
        return 0.0;
    }
    return std::copysign(reed_flow_coefficient(reed) * opening * std::sqrt(std::abs(difference)), difference);
}

bool reed_coupling_is_unique(const ReedChannel& reed, double gain) noexcept
{
    return channel_valid(reed) && gain * reed_flow_coefficient(reed) < std::sqrt(reed.closing_pressure);
}

std::tuple<double, double, double, RootStatus> reed_mouth_step(
    const ReedChannel& reed,
    double& warm_start,
    double end_pressure_gain,
    double mouth_pressure,
    double end_pressure,
    double downstream_flow) noexcept
{
    const double gain = flow_coupling_gain(end_pressure_gain);
    const double target = flow_coupling_target(mouth_pressure, end_pressure, gain, downstream_flow);

    const auto wall = [&](RootStatus status) -> std::tuple<double, double, double, RootStatus> {
        warm_start = 0.0;
        return { centred_end_pressure(mouth_pressure, target, end_pressure), 0.0, target, status };
    };

    if (!channel_valid(reed) || !std::isfinite(target) || !positive_finite(gain)) {
        return wall(RootStatus::NOT_FINITE);
    }

    const double closing = reed.closing_pressure;
    if (target >= closing) {
        return wall(RootStatus::CONVERGED);
    }

    const double magnitude = std::abs(target);
    if (magnitude == 0.0) {
        warm_start = 0.0;
        return { centred_end_pressure(mouth_pressure, 0.0, end_pressure), 0.0, 0.0, RootStatus::CONVERGED };
    }

    const double sign = target > 0.0 ? 1.0 : -1.0;
    const double a = gain * reed_flow_coefficient(reed);
    const double plain_root = 2.0 * magnitude / (a + std::sqrt(a * a + 4.0 * magnitude));

    const double lower = sign > 0.0 ? plain_root : 0.0;
    const double upper = sign > 0.0 ? std::sqrt(magnitude) : plain_root;

    const auto equation = [&](double s) -> RootSample {
        const double cubic = a * s * s * s / closing;
        return {
            s * s + a * s - sign * cubic - magnitude,
            2.0 * s + a - 3.0 * sign * a * s * s / closing
        };
    };

    const double inflection = closing / (3.0 * a);
    double start = warm_start;
    if (sign < 0.0 || upper <= inflection) {
        start = upper;
    } else if (lower >= inflection) {
        start = lower;
    }

    RootOptions options;
    options.check_bracket = false;
    const auto [root, status] = bracketed_newton(equation, lower, upper, start, options);
    warm_start = root;

    const double difference = sign * root * root;
    return {
        centred_end_pressure(mouth_pressure, difference, end_pressure),
        reed_flow(reed, difference),
        difference,
        status
    };
}

std::tuple<double, double, double> dynamic_reed_mouth_step(
    const ReedChannel& reed,
    double resonance,
    double damping,
    double time_step,
    std::array<double, 2>& motion,
    double end_pressure_gain,
    double mouth_pressure,
    double end_pressure,
    double downstream_flow) noexcept
{
    const double gain = flow_coupling_gain(end_pressure_gain);
    const double target = flow_coupling_target(mouth_pressure, end_pressure, gain, downstream_flow);

    if (!channel_valid(reed) || !positive_finite(resonance) || !positive_finite(time_step)
        || !std::isfinite(damping) || damping < 0.0 || !std::isfinite(target) || !positive_finite(gain)) {
        motion = { 0.0, 0.0 };
        return { centred_end_pressure(mouth_pressure, target, end_pressure), 0.0, target };
    }

    const double opening = std::max(0.0, reed.rest_opening + motion[0]);
    const double coefficient = reed.width * opening * std::sqrt(2.0 / reed.air_density);
    const double reach = gain * coefficient;
    const double magnitude = std::abs(target);

    const double root = magnitude > 0.0 ? 2.0 * magnitude / (reach + std::sqrt(reach * reach + 4.0 * magnitude)) : 0.0;
    const double sign = target >= 0.0 ? 1.0 : -1.0;
    const double difference = sign * root * root;
    const double flow = sign * coefficient * root;

    const double omega = 2.0 * std::numbers::pi * resonance;
    const double stiffness = omega * omega;
    const double inverse_square = 1.0 / (time_step * time_step);
    const double half_damping = damping / (2.0 * time_step);
    const double force = -stiffness * reed.rest_opening * difference / reed.closing_pressure;

    const double next = (force + 2.0 * inverse_square * motion[0]
                            - (inverse_square - half_damping + 0.5 * stiffness) * motion[1])
        / (inverse_square + half_damping + 0.5 * stiffness);

    motion = { std::max(next, -reed.rest_opening), motion[0] };
    return { centred_end_pressure(mouth_pressure, difference, end_pressure), flow, difference };
}

} // namespace MayaFlux::Kinesis::Acoustics
