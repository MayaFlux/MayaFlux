#pragma once

/**
 * @file StaggeredLine.hpp
 * @brief Interior propagation of a bore on its interleaved grid, with the
 *        discrete energy and a pickup.
 *
 * Pressure p^n lives at integer steps and volume velocity u^(n+1/2) at half
 * steps, so one sample is: advance_velocity from the current pressure, then
 * the end cells, then advance_pressure for the interior. The end cells are
 * updated by an end law, which is where a reed, a wall or a radiation load
 * acts. Nothing here allocates, and every function runs over spans the
 * caller owns.
 *
 * With a Courant number of one the interior is exact for a cylinder.
 * Below one it adds numerical dispersion and lowers the bandwidth, and it
 * stays stable for any Courant number up to one.
 */

namespace MayaFlux::Kinesis::Acoustics {

/**
 * @brief Advance the half step velocities from the pressure.
 * @param velocity N volume velocities, updated in place
 * @param pressure N + 1 pressures at the start of the sample
 * @param velocity_gain N gains from BoreGrid
 *
 * Each velocity falls by its gain times the pressure rise across it.
 */
MAYAFLUX_API void advance_velocity(
    std::span<double> velocity,
    std::span<const double> pressure,
    std::span<const double> velocity_gain) noexcept;

/**
 * @brief Advance the interior pressures from the new velocities.
 * @param pressure N + 1 pressures, points 1 to N - 1 updated in place
 * @param velocity N volume velocities from advance_velocity
 * @param pressure_gain N + 1 gains from BoreGrid
 *
 * The two end points are left alone for the end laws.
 */
MAYAFLUX_API void advance_pressure(
    std::span<double> pressure,
    std::span<const double> velocity,
    std::span<const double> pressure_gain) noexcept;

/**
 * @brief New pressure of an end cell from the flow into it.
 * @param pressure Pressure of the cell
 * @param pressure_gain Gain of the cell from BoreGrid
 * @param net_inflow Flow in through one face minus flow out through the other
 *
 * With no flow through the outer face this is a rigid wall: pass only the
 * flow arriving from the neighbouring half section.
 */
[[nodiscard]] constexpr double end_cell_pressure(double pressure, double pressure_gain, double net_inflow) noexcept
{
    return pressure + pressure_gain * net_inflow;
}

/**
 * @brief Discrete energy of the bore in joules.
 * @param pressure N + 1 pressures at step n
 * @param velocity_previous N velocities at step n - 1/2
 * @param velocity_current N velocities at step n + 1/2
 * @param pressure_gain N + 1 gains from BoreGrid
 * @param velocity_gain N gains from BoreGrid
 * @param time_step Time step in seconds
 *
 * The sum of cell compliance times pressure squared and section inertance
 * times the product of the velocities at the two time levels, each halved.
 * With sealed ends it is constant to rounding from sample to sample, and
 * with lossy ends it only falls. It is not negative while the Courant number
 * is at most one.
 */
[[nodiscard]] MAYAFLUX_API double discrete_energy(
    std::span<const double> pressure,
    std::span<const double> velocity_previous,
    std::span<const double> velocity_current,
    std::span<const double> pressure_gain,
    std::span<const double> velocity_gain,
    double time_step) noexcept;

/**
 * @brief Pressure at a position along the bore, linearly interpolated.
 * @param pressure N + 1 pressures
 * @param position Fraction of the length, clamped to 0 to 1
 * @return Zero for an empty span
 */
[[nodiscard]] MAYAFLUX_API double sample_pressure(std::span<const double> pressure, double position) noexcept;

} // namespace MayaFlux::Kinesis::Acoustics
