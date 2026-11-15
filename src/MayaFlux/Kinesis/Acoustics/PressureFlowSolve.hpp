#pragma once

#include "MayaFlux/Kinesis/RootFinding.hpp"

/**
 * @file PressureFlowSolve.hpp
 * @brief Same-sample coupling of an end cell to a flow that depends on the
 *        pressure across it.
 *
 * An end cell with an exciter or a load at its edge has a pressure that
 * depends on the flow, and a flow that depends on the pressure. Time
 * centred, the two meet in one scalar equation in the pressure difference
 * d across the coupling:
 *
 *     d + gain * flow(d) = target
 *
 * where gain is half the end cell's pressure gain, target gathers the
 * driving pressure, the cell's pressure and the flow arriving from the bore,
 * and the end pressure afterwards is 2 (driving - d) - end. Solving it in
 * the same sample leaves no one sample delay in the coupling, and being
 * centred in time keeps the end second order and free of added damping.
 *
 * Where the left side is increasing in d the solution is unique. Where the
 * flow law falls with the difference, as a reed does past a third of its
 * closing pressure, uniqueness holds only while gain times the steepest fall
 * stays under one; past that the coupling can have several solutions and
 * the one a search finds follows its starting guess.
 */

namespace MayaFlux::Kinesis::Acoustics {

/** @brief Flow through a coupling, then its slope against the pressure difference. */
using FlowSample = std::pair<double, double>;

/**
 * @brief Gain of a time centred coupling at an end cell.
 * @param end_pressure_gain rho c^2 k / V of the end cell, from BoreGrid
 */
[[nodiscard]] constexpr double flow_coupling_gain(double end_pressure_gain) noexcept
{
    return 0.5 * end_pressure_gain;
}

/**
 * @brief Right side of the coupling equation.
 * @param driving_pressure Pressure on the far side of the coupling at the half step
 * @param end_pressure Pressure of the end cell at the start of the step
 * @param gain From flow_coupling_gain
 * @param downstream_flow Flow from the end cell into the next half section
 *        of the bore, positive away from the coupling
 */
[[nodiscard]] constexpr double flow_coupling_target(
    double driving_pressure, double end_pressure, double gain, double downstream_flow) noexcept
{
    return driving_pressure - end_pressure + gain * downstream_flow;
}

/**
 * @brief Pressure of the end cell after the step, once the difference is solved.
 */
[[nodiscard]] constexpr double centred_end_pressure(
    double driving_pressure, double pressure_difference, double end_pressure) noexcept
{
    return 2.0 * (driving_pressure - pressure_difference) - end_pressure;
}

/**
 * @brief Solve a coupling for any flow law.
 * @param law Callable returning a FlowSample for a pressure difference
 * @param gain From flow_coupling_gain
 * @param target From flow_coupling_target
 * @param lower Lower end of a bracket holding the solution
 * @param upper Upper end of the bracket
 * @param guess Starting difference, usually the previous sample's
 * @param options Stopping rule, see bracketed_newton
 * @return The pressure difference, the flow there, and the search status
 *
 * A flow whose magnitude never exceeds F brackets the solution in
 * [target - gain F, target + gain F]. The bracket must contain a sign change
 * of d + gain * flow(d) - target, which the search checks unless
 * options.check_bracket is off.
 */
template <typename FlowLaw>
    requires std::invocable<FlowLaw&, double>
    && std::convertible_to<std::invoke_result_t<FlowLaw&, double>, FlowSample>
[[nodiscard]] std::tuple<double, double, RootStatus> solve_pressure_flow(
    FlowLaw law, double gain, double target, double lower, double upper, double guess,
    const RootOptions& options = {})
{
    const auto equation = [&](double difference) -> RootSample {
        const auto [flow, slope] = static_cast<FlowSample>(law(difference));
        return { difference + gain * flow - target, 1.0 + gain * slope };
    };

    const auto [difference, status] = bracketed_newton(equation, lower, upper, guess, options);
    return { difference, static_cast<FlowSample>(law(difference)).first, status };
}

} // namespace MayaFlux::Kinesis::Acoustics
