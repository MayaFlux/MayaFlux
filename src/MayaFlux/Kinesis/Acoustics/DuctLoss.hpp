#pragma once

#include "DuctGrid.hpp"

/**
 * @file DuctLoss.hpp
 * @brief Wall losses of a duct as friction and viscosity on the volume velocity.
 *
 * A lossless duct rings forever and keeps every harmonic, which sounds bright,
 * buzzy and saturated. In a real tube the walls take energy from the flow in
 * a boundary layer a fraction of a millimetre thick, and they take more from
 * high frequencies than from low ones. Two terms in the momentum equation
 * stand for that:
 *
 *     du/dt = ... - friction u + viscosity d2u/dx2
 *
 * Friction takes the same fraction of every frequency. Viscosity takes
 * more of the shorter waves: a plane wave of frequency f loses amplitude at
 * rate alpha = friction / (2 c) + viscosity (2 pi f)^2 / (2 c^3) per metre.
 * Between them they can follow the measured loss of a tube over the range that
 * matters, and viscothermal_rates fits the two to the Kirchhoff attenuation at
 * a low and a high frequency. The true loss grows as the square root of the
 * frequency, so this is a fit and not the law; a passive fractional model of
 * the square root is the later refinement.
 *
 * The step is implicit: the velocity after the wave update is replaced by the
 * solution of a tridiagonal system, weighted by each section's inertance, so
 * the step cannot add energy and is stable for any coefficients.
 */

namespace MayaFlux::Kinesis::Acoustics {

/**
 * @brief Plane wave attenuation of a circular tube from its wall boundary layers.
 * @param radius Tube radius in metres
 * @param frequency Frequency in Hz
 * @param sound_speed Speed of sound
 * @param air_density Air density
 * @return Amplitude attenuation in nepers per metre, from the Kirchhoff
 *         low reduced frequency result for air, viscous and thermal terms
 *         together. Zero for a value that is not positive and finite.
 */
[[nodiscard]] MAYAFLUX_API double viscothermal_attenuation(
    double radius, double frequency, double sound_speed = 343.0, double air_density = 1.2) noexcept;

/**
 * @brief Friction and viscosity that match the tube's attenuation at two frequencies.
 * @param radius Tube radius in metres
 * @param sound_speed Speed of sound
 * @param air_density Air density
 * @param low_frequency Lower frequency of the fit in Hz
 * @param high_frequency Upper frequency of the fit in Hz
 * @return Friction in 1/s, then viscosity in m^2/s. Both zero for inputs that
 *         are not positive and finite, or a lower frequency not below the upper.
 */
[[nodiscard]] MAYAFLUX_API std::pair<double, double> viscothermal_rates(
    double radius,
    double sound_speed = 343.0,
    double air_density = 1.2,
    double low_frequency = 150.0,
    double high_frequency = 3000.0) noexcept;

/**
 * @class DuctLoss
 * @brief Prepared implicit loss step for one duct grid.
 */
class MAYAFLUX_API DuctLoss {
public:
    /**
     * @brief Factor the loss for a grid.
     * @param grid Grid the velocities belong to
     * @param friction Friction in 1/s, not negative
     * @param viscosity Viscosity in m^2/s, not negative
     * @return False, leaving the loss as it was, for a grid of fewer than two
     *         intervals or a rate that is negative or not finite
     * @note Allocates only when the grid size changes, so a retune at a block
     *       boundary on the same grid allocates nothing.
     */
    [[nodiscard]] bool configure(const DuctGrid& grid, double friction, double viscosity);

    /**
     * @brief Apply the loss to the velocities of a sample, in place.
     * @param velocity N volume velocities just after the wave update
     *
     * Does nothing when both rates are zero, or when the size does not match
     * the grid. Allocates nothing.
     */
    void apply(std::span<double> velocity) const noexcept;

    [[nodiscard]] bool active() const noexcept { return m_active; }
    [[nodiscard]] double friction() const noexcept { return m_friction; }
    [[nodiscard]] double viscosity() const noexcept { return m_viscosity; }

private:
    std::vector<double> m_mass;
    std::vector<double> m_edge;
    std::vector<double> m_upper;
    std::vector<double> m_inverse_pivot;
    double m_friction {};
    double m_viscosity {};
    bool m_active { false };
};

} // namespace MayaFlux::Kinesis::Acoustics
