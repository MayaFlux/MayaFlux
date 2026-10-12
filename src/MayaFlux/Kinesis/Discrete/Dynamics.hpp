#pragma once

/**
 * @file Dynamics.hpp
 * @brief Time evolution of arrays of independent levels.
 *
 * Every function here takes arrays of numbers and writes an array of levels.
 * What the numbers are, and whether two entries are alike or different, is
 * entirely their content: nothing here has a notion of grouping or
 * agreement, and identical numbers give identical levels.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief Level at a time of a leaky response to a rectangular pulse.
 * @param t Time
 * @param onset Time the pulse begins
 * @param duration Length of the pulse, held to the period when one is given
 * @param rise Time constant while the pulse is on; zero or less jumps to one
 * @param fall Time constant after the pulse ends; zero or less falls to zero at once
 * @param period Repeat interval; zero or less means a single pulse
 * @return Level in [0, 1]
 *
 * During the pulse the level climbs toward one as 1 - exp(-s / rise), with s
 * the time since the pulse began. After it ends the level decays from where it
 * stood as exp(-s / fall). Before the onset it is zero. With a period, the
 * pattern repeats from rest every period, so a tail still decaying when the
 * next pulse begins is cut off there.
 */
[[nodiscard]] MAYAFLUX_API double pulse_response(
    double t, double onset, double duration, double rise, double fall, double period = 0.0) noexcept;

/**
 * @brief pulse_response over arrays, written to out.
 * @param out Levels, one per entry
 * @param t Time
 * @param onset Onset of each entry
 * @param duration Pulse length of each entry
 * @param rise Rise constant of each entry
 * @param fall Fall constant of each entry
 * @param period Repeat interval of each entry, zero or less for a single pulse
 *
 * Runs over the shortest of the spans. A closed form in t, so any time can be
 * asked for in any order and nothing is carried between calls.
 */
MAYAFLUX_API void pulse_response(
    std::span<double> out,
    double t,
    std::span<const double> onset,
    std::span<const double> duration,
    std::span<const double> rise,
    std::span<const double> fall,
    std::span<const double> period) noexcept;

/**
 * @brief One explicit step of generalised Lotka-Volterra competition.
 * @param levels Nonnegative levels, updated in place
 * @param growth Growth rate of each level
 * @param interaction Row-major N x N matrix; entry (i, j) is how strongly
 *        level j suppresses level i, the diagonal being self limitation
 * @param dt Step
 *
 * Each level changes by dt * a_i * (growth_i - sum_j interaction_ij * a_j),
 * all from the levels at the start of the step, then is held at zero or
 * above. With suppression stronger between entries than within them, the
 * levels take turns instead of settling together. Does nothing when the
 * interaction matrix is smaller than N x N. Uses a per-thread scratch
 * vector, so it allocates only the first time a size is seen.
 */
MAYAFLUX_API void lotka_volterra_step(
    std::span<double> levels,
    std::span<const double> growth,
    std::span<const double> interaction,
    double dt);

} // namespace MayaFlux::Kinesis::Discrete
