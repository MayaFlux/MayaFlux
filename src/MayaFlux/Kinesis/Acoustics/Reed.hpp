#pragma once

#include "PressureFlowSolve.hpp"

/**
 * @file Reed.hpp
 * @brief Quasi-static single reed and its same-sample coupling to the bore.
 *
 * The reed channel opens by H (1 - d / pM) for a mouth to bore pressure
 * difference d, and shuts once d reaches the closing pressure pM. Through
 * the opening the flow is Bernoulli:
 *
 *     U(d) = w H (1 - d / pM) sign(d) sqrt(2 |d| / rho)
 *
 * It rises with d until a third of pM, then falls to zero at pM, and that
 * falling part is what lets the bore drive the reed into oscillation.
 *
 * Coupled to the first cell of the bore the equation d + gain U(d) = target
 * has a slope of infinity at d = 0 in d, which stalls Newton steps there.
 * With s = sign(d) sqrt(|d|), so d = s |s|, the flow is a polynomial in s,
 *
 *     s |s| + gain w H sqrt(2 / rho) s (1 - s |s| / pM) = target
 *
 * which is smooth through zero, and for either sign of the target the
 * solution is held in an interval known beforehand, so the search is
 * bracketed from its first step and typically needs two or three
 * evaluations from the previous sample's solution.
 */

namespace MayaFlux::Kinesis::Acoustics {

/**
 * @struct ReedChannel
 * @brief Geometry and constants of the reed channel.
 */
struct ReedChannel {
    double width {};
    double rest_opening {};
    double closing_pressure {};
    double air_density { 1.2 };
};

/**
 * @brief w H sqrt(2 / rho), the flow per unit of (1 - d / pM) sign(d) sqrt|d|.
 */
[[nodiscard]] MAYAFLUX_API double reed_flow_coefficient(const ReedChannel& reed) noexcept;

/**
 * @brief Volume velocity through the channel for a pressure difference.
 * @param reed Channel
 * @param difference Mouth pressure minus bore pressure
 * @return Zero from the closing pressure up, and for a channel with any
 *         non-positive or non-finite field
 */
[[nodiscard]] MAYAFLUX_API double reed_flow(const ReedChannel& reed, double difference) noexcept;

/**
 * @brief Whether a coupling gain keeps the solution unique.
 * @param reed Channel
 * @param gain From flow_coupling_gain
 * @return True while gain times the flow coefficient stays under the root of
 *         the closing pressure, which is zeta lambda under one
 *
 * Past it the coupling can have more than one solution, and the one found
 * depends on the previous sample's. Check it whenever the channel, the
 * length or the sample rate changes.
 */
[[nodiscard]] MAYAFLUX_API bool reed_coupling_is_unique(const ReedChannel& reed, double gain) noexcept;

/**
 * @brief Couple the reed to the first cell of the bore for one sample.
 * @param reed Channel
 * @param warm_start Root of |d| from the previous sample, updated here; start
 *        it at zero. The search begins at the end of its bracket from which
 *        Newton steps approach the root monotonically, which is every case
 *        but a root straddling the inflection of the equation, where it
 *        begins here.
 * @param end_pressure_gain Gain of the first pressure point from DuctGrid
 * @param mouth_pressure Blowing pressure at the half step
 * @param end_pressure Pressure of the first point at the start of the sample
 * @param downstream_flow Volume velocity of the first half section, from the
 *        same sample
 * @return The first point's pressure after the sample, the flow into the bore
 *         at the half step, the pressure difference across the reed, and how
 *         the search ended
 *
 * A reed shut by the mouth pressure acts as a wall. A channel with a field
 * that is not positive and finite does the same and reports NOT_FINITE.
 * The result is the one solution only while reed_coupling_is_unique holds
 * for the coupling gain, flow_coupling_gain of end_pressure_gain.
 */
[[nodiscard]] MAYAFLUX_API std::tuple<double, double, double, RootStatus> reed_mouth_step(
    const ReedChannel& reed,
    double& warm_start,
    double end_pressure_gain,
    double mouth_pressure,
    double end_pressure,
    double downstream_flow) noexcept;

/**
 * @brief Couple a reed with its own dynamics to the first cell of the bore.
 * @param reed Channel. closing_pressure sets the static compliance, so the
 *        reed settles at H (1 - d / pM) under a steady difference d.
 * @param resonance Resonance of the reed in Hz, positive
 * @param damping Damping of the reed in 1/s, not negative
 * @param reed_area Effective area in square metres over which the reed moves
 *        air, not negative; zero leaves the reed's own flow out
 * @param time_step Time step in seconds
 * @param motion Displacement of the reed from rest, then the previous one, in
 *        metres, updated here; start both at zero
 * @param end_pressure_gain Gain of the first pressure point from DuctGrid
 * @param mouth_pressure Blowing pressure at the half step
 * @param end_pressure Pressure of the first point at the start of the sample
 * @param downstream_flow Volume velocity of the first half section, from the
 *        same sample
 * @return The first point's pressure after the sample, the flow into the bore
 *         at the half step, and the pressure difference across the reed
 *
 * The reed is a damped oscillator, y'' + damping y' + w0^2 y = -w0^2 H d / pM,
 * stepped by a centred scheme that is stable for any step. The opening during
 * a sample is the one the previous sample left, which makes the coupling an
 * exact closed form, s |s| + a s = target with s = sign(d) sqrt|d|, solved by
 * its stable root. The solution is unique for every parameter, with no
 * condition to check. The lag is half a sample, small beside the period of a
 * reed resonance. A reed driven past the lay is held there without bouncing.
 * With a reed_area the moving reed also pushes air, a flow of -reed_area times
 * its velocity into the bore, which is added to the flow returned and enters
 * the coupling like any other known flow (Avanzini and Rocchesso). An invalid
 * channel or resonance acts as a wall.
 */
[[nodiscard]] MAYAFLUX_API std::tuple<double, double, double> dynamic_reed_mouth_step(
    const ReedChannel& reed,
    double resonance,
    double damping,
    double reed_area,
    double time_step,
    std::array<double, 2>& motion,
    double end_pressure_gain,
    double mouth_pressure,
    double end_pressure,
    double downstream_flow) noexcept;

} // namespace MayaFlux::Kinesis::Acoustics
