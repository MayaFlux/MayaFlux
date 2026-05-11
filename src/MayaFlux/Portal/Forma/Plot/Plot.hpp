#pragma once

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"

namespace MayaFlux::Portal::Forma::Plot {

// =============================================================================
// AxisRange
// =============================================================================

/**
 * @struct AxisRange
 * @brief Scalar domain extent for one plot axis.
 *
 * Maps raw double values in [min, max] to the normalised render space
 * expected by the geometry function. When auto_scale is true, min and max
 * are recomputed from the series data on every process() call and the
 * stored values are ignored until then.
 */
struct AxisRange {
    float min { -1.F };
    float max { 1.F };
    bool auto_scale { false };

    /** @brief Map a value into [0, 1] within this range. */
    [[nodiscard]] float normalise(float v) const noexcept
    {
        if (max == min)
            return 0.F;
        return (v - min) / (max - min);
    }

    /** @brief Map a value into [-1, 1] NDC within this range. */
    [[nodiscard]] float to_ndc(float v) const noexcept
    {
        return normalise(v) * 2.F - 1.F;
    }
};

// =============================================================================
// series_by_role
// =============================================================================

/**
 * @brief Collect all series from processed_data whose DataDimension role
 *        matches @p role.
 *
 * Returns spans pointing directly into the DataVariant storage — no copy.
 * Returns an empty vector if no series carry the role or if any matching
 * variant does not hold vector<double>.
 *
 * @param container  PlotContainer after process_default() has been called.
 * @param role       DataDimension::Role to match.
 */
[[nodiscard]] std::vector<std::span<const double>> series_by_role(
    const Kakshya::PlotContainer& container,
    Kakshya::DataDimension::Role role);

/**
 * @brief Compute [min, max] over a scalar series.
 *
 * Returns {0, 1} for an empty span.
 */
[[nodiscard]] std::pair<float, float> data_range(std::span<const double> series);

/**
 * @brief Apply auto-scaling to an AxisRange from a set of series.
 *
 * Computes the union [min, max] over all provided series and updates
 * @p range in place. No-op if series is empty or range.auto_scale is false.
 */
void apply_auto_scale(AxisRange& range,
    const std::vector<std::span<const double>>& series);

// =============================================================================
// Palette
// =============================================================================

/**
 * @brief Resolve a per-series color from a palette.
 *
 * Wraps @p palette cyclically: palette[index % palette.size()].
 * Returns white if @p palette is empty.
 *
 * @param palette  Per-series base colors. Size 1 = uniform color.
 * @param index    Series index.
 */
[[nodiscard]] glm::vec3 palette_color(const std::vector<glm::vec3>& palette,
    size_t index) noexcept;

// =============================================================================
// Geometry function factories
// =============================================================================

/**
 * @brief LINE_STRIP waveform for TIME or SPATIAL_Y series.
 *
 * Reads all SPATIAL_X series as explicit X sample positions. If none are
 * present, generates uniform X positions across [x_range.min, x_range.max]
 * for each Y series independently.
 *
 * SPATIAL_Z series map to the Z position of each vertex if present,
 * enabling 3D ribbon / parametric curve display. If a SPATIAL_Z series
 * count does not match the corresponding Y series, Z defaults to 0.
 *
 * Each Y series produces one LINE_STRIP run. Runs are separated by a
 * degenerate vertex so they share one FormaBuffer.
 *
 * Per-series color is resolved from @p palette cyclically.
 *
 * Calls container->process_default() before reading processed_data.
 *
 * @param x_range   Domain for X. auto_scale recomputes from data each frame.
 * @param y_range   Domain for Y.
 * @param z_range   Domain for Z. Ignored if no SPATIAL_Z series present.
 * @param palette   Per-series colors, cycled. {1,1,1} if empty.
 * @param thickness Line thickness in pixels.
 */
[[nodiscard]] GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> waveform(
    AxisRange x_range = {},
    AxisRange y_range = {},
    AxisRange z_range = {},
    const std::vector<glm::vec3>& palette = {},
    float thickness = 1.5F);

/**
 * @brief POINT_LIST scatter plot for paired (X, Y[, Z]) series.
 *
 * Pairs the i-th SPATIAL_X series with the i-th SPATIAL_Y series. If there
 * are more Y series than X series, the last X series is reused. Each pair
 * produces one run of PointVertex in the output bytes.
 *
 * SPATIAL_Z series map to Z position, enabling a true 3D scatter cloud.
 * A COLOR series modulates per-sample point color: samples are mapped
 * through y_range.normalise() and blended between white and the series
 * base color from @p palette.
 *
 * Per-series color is resolved from @p palette cyclically.
 *
 * @param x_range    Domain for X.
 * @param y_range    Domain for Y.
 * @param z_range    Domain for Z.
 * @param palette    Per-series colors, cycled. {1,1,1} if empty.
 * @param point_size Point size in pixels.
 */
[[nodiscard]] GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> scatter(
    AxisRange x_range = {},
    AxisRange y_range = {},
    AxisRange z_range = {},
    const std::vector<glm::vec3>& palette = {},
    float point_size = 4.F);

/**
 * @brief TRIANGLE_LIST bar chart for FREQUENCY or SPATIAL_Y series.
 *
 * Each FREQUENCY series produces one set of bars tiled uniformly across
 * [x_range.min, x_range.max]. Bar height maps sample value through y_range.
 *
 * Multiple FREQUENCY series are currently rendered on top of each other
 * in the same X range. Side-by-side or stacked layout is not yet
 * implemented; no three-caller evidence exists for those layouts.
 *
 * Per-series color is resolved from @p palette cyclically.
 *
 * @param x_range  Domain for X (bar positions).
 * @param y_range  Domain for Y (bar heights).
 * @param palette  Per-series colors, cycled. {1,1,1} if empty.
 */
[[nodiscard]] GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> bars(
    AxisRange x_range = {},
    AxisRange y_range = {},
    const std::vector<glm::vec3>& palette = {});

/**
 * @brief TRIANGLE_STRIP background quad for a plot area.
 *
 * Produces a solid-color or textured full-screen quad covering @p bounds.
 * Intended to be added to the same Layer as a plot element via Layer::relate,
 * rendered before it.
 *
 * When @p texture is non-null the quad uses the textured vertex path and
 * the caller must pass a matching FormaBuffer with a texture binding. When
 * null the quad is filled with @p color.
 *
 * T is float (a dummy tick value — the background is static unless the
 * caller drives it). Write any float to state->write() to refresh.
 *
 * @param bounds   Plot area in NDC.
 * @param color    Fill color when no texture is provided.
 * @param texture  Optional GPU image. nullptr = solid color.
 */
[[nodiscard]] GeometryFn<float> background(
    Kinesis::AABB2D bounds,
    glm::vec3 color = glm::vec3(1.F),
    const std::shared_ptr<Core::VKImage>& texture = nullptr);

} // namespace MayaFlux::Portal::Forma::Plot
