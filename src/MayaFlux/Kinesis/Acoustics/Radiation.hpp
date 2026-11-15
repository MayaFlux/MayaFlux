#pragma once

/**
 * @file Radiation.hpp
 * @brief Open end of a bore as a passive second order radiation load.
 *
 * The end sees an inertance L, standing for the mass of air moving just
 * outside, in parallel with a series pair: a resistance R1 equal to the
 * bore's characteristic impedance, then a resistance R2 in parallel with a
 * capacitance C. Low frequencies are reflected by L, which gives the end
 * correction, and high frequencies pass through R1 into the room. This is
 * the lumped network of Bilbao and Chick, and its impedance is exactly the
 * low order Pade fit to the Levine and Schwinger reflection coefficient
 * given by Silva et al., with
 *
 *     L = lambda (a / c) Zc,  R1 = Zc,  R2 = gamma Zc,  C = theta (a / c) / Zc
 *
 * for end radius a and bore impedance Zc = rho c / S. The model is
 * dimensionless in gamma, lambda and theta, so a flanged end is another
 * set of three numbers.
 *
 * The update is the trapezoidal rule on every element, in the end pressure
 * averaged over the step. That is exact for the energy of a linear passive
 * network, so the load can only absorb energy, and the end cell and the
 * load are solved together in closed form with no iteration.
 */

namespace MayaFlux::Kinesis::Acoustics {

/**
 * @brief The three dimensionless constants of the radiation network, as
 *        gamma, lambda and theta.
 */
using RadiationModel = std::array<double, 3>;

/**
 * @brief Network constants from the numerator and denominator of the Pade
 *        reflection coefficient R = -(1 - n1 s) / (1 - d1 s + d2 s^2).
 *
 * With s = j k a. Fits the model's impedance to that reflection, matching
 * both the end correction and the radiation resistance at low frequency.
 * The mapping is solved from the impedance rather than taken from Eq. (20)
 * of Silva et al., which carries an erratum.
 */
[[nodiscard]] constexpr RadiationModel radiation_model_from_pade(double n1, double d1, double d2) noexcept
{
    const double lambda = 0.5 * (d1 - n1);
    const double gamma = 2.0 * lambda * lambda / (lambda * (d1 + n1) - d2) - 1.0;
    const double theta = (0.5 * (d1 + n1) * (1.0 + gamma) - lambda) / gamma;
    return { gamma, lambda, theta };
}

/** @brief Unflanged circular pipe, end correction 0.6133 a. Accurate to about 8 percent in modulus up to ka of 2. */
inline constexpr RadiationModel unflanged_radiation = radiation_model_from_pade(0.167, 1.393, 0.457);

/** @brief Pipe in an infinite flange, end correction 0.8216 a. */
inline constexpr RadiationModel flanged_radiation = radiation_model_from_pade(0.182, 1.825, 0.649);

/**
 * @struct RadiationSpec
 * @brief What the network constants are scaled by.
 */
struct RadiationSpec {
    RadiationModel model { unflanged_radiation };
    double radius {};
    double sound_speed { 343.0 };
    double air_density { 1.2 };
    double time_step {};
};

/**
 * @struct RadiationCoefficients
 * @brief Prepared coefficients of one end, built once per bore or step change.
 */
struct RadiationCoefficients {
    double inductor_gain {};
    double branch_weight {};
    double branch_memory {};
    double memory_conductance {};
    double conductance {};
    double end_half_gain {};
    double reciprocal {};
};

/**
 * @struct RadiationState
 * @brief Inductor flow and branch pressure, zero at rest.
 */
struct RadiationState {
    double inductor_flow {};
    double branch_pressure {};
};

/**
 * @brief Prepare the coefficients of a radiating end.
 * @param spec Network constants, end radius, air constants and time step
 * @param end_area Area of the bore at the end plane, pi a^2 of the end radius
 * @param end_pressure_gain Gain of the last pressure point from DuctGrid
 * @return The coefficients, or nothing when any value is not positive and finite
 */
[[nodiscard]] MAYAFLUX_API std::optional<RadiationCoefficients> make_radiation_coefficients(
    const RadiationSpec& spec, double end_area, double end_pressure_gain) noexcept;

/**
 * @brief Advance the end cell and the load one sample.
 * @param coefficients From make_radiation_coefficients
 * @param state Updated in place
 * @param end_pressure Pressure of the last point at the start of the sample
 * @param upstream_flow Volume velocity of the last half section, from the same sample
 * @return The end pressure after the sample, then the flow out of the bore at
 *         the half step
 */
[[nodiscard]] inline std::pair<double, double> radiation_step(
    const RadiationCoefficients& coefficients,
    RadiationState& state,
    double end_pressure,
    double upstream_flow) noexcept
{
    const double mean = (end_pressure
                            + coefficients.end_half_gain
                                * (upstream_flow - state.inductor_flow
                                    + coefficients.memory_conductance * state.branch_pressure))
        * coefficients.reciprocal;

    const double flow = state.inductor_flow
        - coefficients.memory_conductance * state.branch_pressure
        + coefficients.conductance * mean;

    const double branch_mean = coefficients.branch_weight * mean
        + coefficients.branch_memory * state.branch_pressure;

    state.inductor_flow += coefficients.inductor_gain * mean;
    state.branch_pressure = 2.0 * branch_mean - state.branch_pressure;

    return { 2.0 * mean - end_pressure, flow };
}

/**
 * @brief Pressure radiated into free space by a flow out of the end.
 * @param flow Flow at the current sample
 * @param previous_flow Flow at the previous sample
 * @param time_step Time step in seconds
 * @param air_density Air density
 * @param distance Distance from the end in metres
 * @return rho / (4 pi r) times the rate of change of flow, a point source in
 *         a free field
 */
[[nodiscard]] constexpr double radiated_pressure(
    double flow, double previous_flow, double time_step, double air_density, double distance = 1.0) noexcept
{
    return air_density / (4.0 * std::numbers::pi * distance) * (flow - previous_flow) / time_step;
}

} // namespace MayaFlux::Kinesis::Acoustics
