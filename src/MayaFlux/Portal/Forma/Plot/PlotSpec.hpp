#pragma once

#include "AxisRange.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"

namespace MayaFlux::Portal::Forma {
class Surface;
}

namespace MayaFlux::Kakshya {
class PlotContainer;
}

namespace MayaFlux::Portal::Forma::Plot {

struct SeriesSpec;

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

// =============================================================================
// Grid
//
// Static LINE_LIST geometry for NDC-space grid lines. Produces x_divisions
// vertical lines and y_divisions horizontal lines within bounds. Use as a
// static FormaBuffer submitted once and related behind the data element.
//
// Caller pattern:
//   auto grid_buf = Portal::Forma::create_buffer(window,
//       Plot::plot_grid(bounds, 8, 6),
//       Portal::Graphics::PrimitiveTopology::LINE_LIST);
//   surface.layer().add(
//       Portal::Forma::Element{}.non_interactive().with_buffer(grid_buf))
//       .relate_to(data_el.element.id).to_back();
// =============================================================================

/**
 * @brief LINE_LIST grid geometry for a plot area.
 *
 * Produces @p x_divisions vertical lines and @p y_divisions horizontal lines
 * evenly distributed within @p bounds. Intended as a static background buffer.
 *
 * @param bounds      Plot area in NDC.
 * @param x_divisions Vertical line count (columns). 0 = no vertical lines.
 * @param y_divisions Horizontal line count (rows). 0 = no horizontal lines.
 * @param color       Line color.
 * @param thickness   LineVertex::thickness value.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> plot_grid(
    Kinesis::AABB2D bounds,
    uint32_t x_divisions,
    uint32_t y_divisions,
    glm::vec3 color = glm::vec3(0.12F),
    float thickness = 1.F);

// =============================================================================
// Cursor / playhead
//
// GeometryFn<float>. Value in [0, 1] maps to a normalised position within
// bounds along the active axis. vertical=true draws a vertical line (X axis
// position); vertical=false draws a horizontal line (Y axis position).
//
// Produces two LINE_LIST LineVertex pairs. Topology: LINE_LIST.
//
// Caller pattern:
//   auto cursor = Portal::Forma::create_element<float>(
//       surface,
//       Plot::plot_cursor(bounds),
//       0.F,
//       Portal::Graphics::PrimitiveTopology::LINE_LIST);
//   // drive from a metro or bridge binding
//   cursor.state->write(playhead_position);
// =============================================================================

/**
 * @brief GeometryFn<float> for a cursor or playhead line within a plot area.
 *
 * Value in [0, 1] maps to a position within @p bounds. When @p vertical is
 * true the line is vertical and the value maps to the X axis; when false the
 * line is horizontal and the value maps to the Y axis.
 *
 * @note Pass @c PrimitiveTopology::LINE_LIST explicitly to create_element.
 *
 * @param bounds    Plot area in NDC. The line spans the full perpendicular extent.
 * @param vertical  True for a vertical playhead (X position), false for horizontal.
 * @param color     Line color.
 * @param thickness LineVertex::thickness value.
 */
[[nodiscard]] GeometryFn<float> plot_cursor(
    Kinesis::AABB2D bounds,
    bool vertical = true,
    glm::vec3 color = glm::vec3(0.75F),
    float thickness = 1.F);

} // namespace MayaFlux::Portal::Forma::Plot
