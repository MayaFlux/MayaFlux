#pragma once

#include <expected>

/**
 * @file DuctGrid.hpp
 * @brief Interleaved grid of a one dimensional acoustic bore.
 *
 * Pressure sits at N + 1 points and volume velocity at the N half points
 * between them. Each pressure point owns the volume of the two half sections
 * around it, so the compliance of a cell is its volume over rho c^2, and the
 * end cells are half cells. The interior and end updates all draw their
 * gains from the two arrays prepared here.
 *
 * Nothing here logs or reads a global sample rate; every value comes from
 * the spec.
 */

namespace MayaFlux::Kinesis::Acoustics {

/**
 * @struct DuctSpec
 * @brief Physical and sampling constants a grid is built from.
 *
 * mouth_volume is extra air behind the first cell in cubic metres, which adds
 * compliance there. It stands for a mouthpiece cavity, or for the tip a
 * truncated cone lacks, whose volume is pi r^2 x / 3 for the apex radius r and
 * the distance x to the apex. Without it the modes of a truncated cone are
 * stretched away from a whole number series.
 */
struct DuctSpec {
    double length {};
    double sample_rate {};
    double sound_speed { 343.0 };
    double air_density { 1.2 };
    double mouth_volume { 0.0 };
};

/**
 * @enum DuctError
 * @brief Why no grid was built.
 *
 * INVALID_SPEC covers a length, rate, sound speed or density that is not
 * positive and finite, or a grid beyond sixteen million intervals.
 * INVALID_PROFILE covers a radius that is not positive and finite, or fewer
 * than two samples. TOO_SHORT is a valid spec that gives fewer than two
 * intervals.
 */
enum class DuctError : uint8_t {
    INVALID_SPEC,
    INVALID_PROFILE,
    TOO_SHORT
};

/**
 * @struct DuctGrid
 * @brief Prepared grid and update gains of one bore.
 *
 * With N = intervals, area and velocity_gain hold N values at the half points
 * and pressure_gain holds N + 1 values at the pressure points.
 * velocity_gain is k S / (rho h) and pressure_gain is rho c^2 k / V, with V the
 * volume of the cell, for time step k and spacing h. courant is c k / h, at
 * most one, and bandwidth is the highest frequency the grid carries,
 * fs asin(courant) / pi.
 */
struct DuctGrid {
    size_t intervals {};
    double spacing {};
    double time_step {};
    double courant {};
    double bandwidth {};
    double start_radius {};
    double end_radius {};
    std::vector<double> area;
    std::vector<double> velocity_gain;
    std::vector<double> pressure_gain;
};

/**
 * @brief Number of grid intervals a spec allows.
 * @return floor(length / (c k)), the most that keeps c k / h at most one, or
 *         zero when the spec is invalid
 */
[[nodiscard]] MAYAFLUX_API size_t duct_intervals(const DuctSpec& spec) noexcept;

/**
 * @brief Build a grid from radii sampled evenly along the bore.
 * @param spec Length, sample rate and air constants
 * @param radii At least two radii, evenly spaced from the start to the end
 *        of the bore. They are linearly interpolated to the half points.
 * @return The grid, or why none was built
 *
 * The grid spacing is length / N with N the largest count keeping the
 * Courant number at most one. A length that is not a whole number of
 * intervals leaves it a little under one, which lowers the bandwidth.
 */
[[nodiscard]] MAYAFLUX_API std::expected<DuctGrid, DuctError> make_duct_grid(
    const DuctSpec& spec, std::span<const double> radii);

namespace detail {

    [[nodiscard]] MAYAFLUX_API std::expected<DuctGrid, DuctError> begin_duct_grid(const DuctSpec& spec);

    [[nodiscard]] MAYAFLUX_API std::expected<DuctGrid, DuctError> finish_duct_grid(
        DuctGrid grid, const DuctSpec& spec);

}

/**
 * @brief Build a grid from a radius function of position.
 * @param spec Length, sample rate and air constants
 * @param radius Callable giving the radius in metres at a distance in metres
 *        from the start, evaluated at the half points and at both ends
 * @return As the span overload
 */
template <typename Radius>
    requires std::is_invocable_r_v<double, Radius&, double>
[[nodiscard]] std::expected<DuctGrid, DuctError> make_duct_grid(const DuctSpec& spec, Radius radius)
{
    auto begun = detail::begin_duct_grid(spec);
    if (!begun) {
        return begun;
    }

    DuctGrid grid = std::move(*begun);
    for (size_t l = 0; l < grid.intervals; ++l) {
        const double r = radius((static_cast<double>(l) + 0.5) * grid.spacing);
        grid.area[l] = std::numbers::pi * r * r;
    }
    grid.start_radius = radius(0.0);
    grid.end_radius = radius(spec.length);

    return detail::finish_duct_grid(std::move(grid), spec);
}

} // namespace MayaFlux::Kinesis::Acoustics
