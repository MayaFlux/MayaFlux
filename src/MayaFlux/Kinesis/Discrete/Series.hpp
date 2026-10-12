#pragma once

/**
 * @file Series.hpp
 * @brief Ordered sequences of positive numbers.
 *
 * Each function returns a sequence relative to a base value, 1 by default, so
 * the result reads as ratios and scales to any unit by that base. They are
 * plain number producers: what the numbers are used for is not part of them.
 */

namespace MayaFlux::Kinesis::Discrete {

/**
 * @brief base * 1, base * 2, ... base * count.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> harmonic_series(size_t count, double base = 1.0);

/**
 * @brief Integers pulled outward with order: base * n * sqrt(1 + stretch * n^2).
 * @param count Number of terms, n running from 1
 * @param stretch Zero gives the harmonic series; positive values spread the upper terms
 * @param base Value of the first term when stretch is zero
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> stretched_series(
    size_t count, double stretch, double base = 1.0);

/**
 * @brief Ratios of the transverse modes of a uniform free-free beam.
 * @param count Number of terms
 * @param base Value of the first term
 *
 * The squares of the roots of cos(x) cosh(x) = 1, scaled so the first is
 * one: 1, 2.756, 5.404, 8.933, 13.344, and so on, found to machine
 * precision rather than from a table.
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> free_bar_series(size_t count, double base = 1.0);

/**
 * @brief Geometric steps: base * ratio^(k * step) for k = 1 .. count.
 * @param count Number of terms
 * @param ratio The number raised to the power; the golden ratio gives a
 *        quasi-periodic spacing
 * @param step Exponent increment per term
 * @param base Scale
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> power_series(
    size_t count, double ratio, double step, double base = 1.0);

/**
 * @brief Each value replaced by a close pair, v * (1 - spread) and v * (1 + spread).
 * @param values Sequence to split; the result has twice as many terms, each
 *        pair adjacent and ascending
 * @param spread Relative separation, for example 0.001 for a thousandth
 */
[[nodiscard]] MAYAFLUX_API std::vector<double> split_pairs(
    std::span<const double> values, double spread);

} // namespace MayaFlux::Kinesis::Discrete
