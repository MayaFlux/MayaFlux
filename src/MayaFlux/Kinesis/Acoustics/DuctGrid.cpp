#include "DuctGrid.hpp"

namespace MayaFlux::Kinesis::Acoustics {

namespace {

    constexpr size_t minimum_intervals = 2;
    constexpr double maximum_intervals = 16777216.0;

    [[nodiscard]] bool positive_finite(double value) noexcept
    {
        return std::isfinite(value) && value > 0.0;
    }

    [[nodiscard]] std::optional<double> interval_count(const DuctSpec& spec) noexcept
    {
        if (!positive_finite(spec.length) || !positive_finite(spec.sample_rate)
            || !positive_finite(spec.sound_speed) || !positive_finite(spec.air_density)
            || !std::isfinite(spec.mouth_volume) || spec.mouth_volume < 0.0) {
            return std::nullopt;
        }

        const double count = std::floor(spec.length * spec.sample_rate / spec.sound_speed);
        return count < maximum_intervals ? std::optional<double> { count } : std::nullopt;
    }

}

size_t duct_intervals(const DuctSpec& spec) noexcept
{
    return static_cast<size_t>(interval_count(spec).value_or(0.0));
}

namespace detail {

    std::expected<DuctGrid, DuctError> begin_duct_grid(const DuctSpec& spec)
    {
        const std::optional<double> count = interval_count(spec);
        if (!count) {
            return std::unexpected(DuctError::INVALID_SPEC);
        }

        const auto intervals = static_cast<size_t>(*count);
        if (intervals < minimum_intervals) {
            return std::unexpected(DuctError::TOO_SHORT);
        }

        DuctGrid grid;
        grid.intervals = intervals;
        grid.spacing = spec.length / static_cast<double>(intervals);
        grid.time_step = 1.0 / spec.sample_rate;
        grid.courant = std::min(1.0, spec.sound_speed * grid.time_step / grid.spacing);
        grid.bandwidth = spec.sample_rate * std::asin(grid.courant) / std::numbers::pi;
        grid.area.assign(intervals, 0.0);
        return grid;
    }

    std::expected<DuctGrid, DuctError> finish_duct_grid(DuctGrid grid, const DuctSpec& spec)
    {
        const bool profile_valid = positive_finite(grid.start_radius) && positive_finite(grid.end_radius)
            && std::ranges::all_of(grid.area, positive_finite);
        if (!profile_valid) {
            return std::unexpected(DuctError::INVALID_PROFILE);
        }

        const size_t intervals = grid.intervals;
        const double k = grid.time_step;
        const double h = grid.spacing;
        const double rho = spec.air_density;
        const double stiffness = rho * spec.sound_speed * spec.sound_speed * k;

        grid.velocity_gain.resize(intervals);
        grid.pressure_gain.resize(intervals + 1);

        for (size_t l = 0; l < intervals; ++l) {
            grid.velocity_gain[l] = k * grid.area[l] / (rho * h);
        }

        grid.pressure_gain[0] = stiffness / (0.5 * h * grid.area[0] + spec.mouth_volume);
        for (size_t l = 1; l < intervals; ++l) {
            grid.pressure_gain[l] = stiffness / (0.5 * h * (grid.area[l - 1] + grid.area[l]));
        }
        grid.pressure_gain[intervals] = stiffness / (0.5 * h * grid.area[intervals - 1]);

        return grid;
    }

}

std::expected<DuctGrid, DuctError> make_duct_grid(const DuctSpec& spec, std::span<const double> radii)
{
    auto begun = detail::begin_duct_grid(spec);
    if (!begun) {
        return begun;
    }

    if (radii.size() < 2 || !std::ranges::all_of(radii, positive_finite)) {
        return std::unexpected(DuctError::INVALID_PROFILE);
    }

    DuctGrid grid = std::move(*begun);
    const double last = static_cast<double>(radii.size() - 1);
    for (size_t l = 0; l < grid.intervals; ++l) {
        const double position = (static_cast<double>(l) + 0.5) / static_cast<double>(grid.intervals) * last;
        const size_t index = std::min(static_cast<size_t>(position), radii.size() - 2);
        const double fraction = position - static_cast<double>(index);
        const double r = radii[index] + fraction * (radii[index + 1] - radii[index]);
        grid.area[l] = std::numbers::pi * r * r;
    }
    grid.start_radius = radii.front();
    grid.end_radius = radii.back();

    return detail::finish_duct_grid(std::move(grid), spec);
}

} // namespace MayaFlux::Kinesis::Acoustics
