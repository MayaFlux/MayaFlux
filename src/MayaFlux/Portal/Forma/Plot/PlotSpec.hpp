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

/**
 * @brief Edge along which tick labels are placed.
 */
enum class TickEdge : uint8_t {
    Bottom, ///< Labels below bounds, values mapped from range.min (left) to range.max (right).
    Top, ///< Labels above bounds.
    Left, ///< Labels left of bounds, values mapped from range.min (bottom) to range.max (top).
    Right, ///< Labels right of bounds.
};

/**
 * @brief Construction-free text label description.
 *
 * This is only a layout/style spec. It does not own a FormaBuffer, VKImage,
 * Element, Surface, or Window. Forma is responsible for turning this into
 * a textured Element via Element::with_text().
 */
struct LabelSpec {
    std::string text;
    Kinesis::AABB2D bounds {};
    glm::vec4 color { 0.85F, 0.85F, 0.85F, 1.F };

    /**
     * @brief Optional logical name for the eventual Element.
     */
    std::string name;

    /**
     * @brief Whether the eventual Element should participate in hit testing.
     * Plot labels default to passive/non-interactive.
     */
    bool interactive { false };

    /**
     * @brief Optional text pixel/render size override.
     *
     * When zero, Forma should derive render_bounds from bounds + window size.
     */
    glm::uvec2 render_bounds {};
};

/**
 * @brief Lightweight filled rectangle description.
 *
 * Used for plot adornments such as legend swatches. Construction-free:
 * Forma turns this into a buffer/Element later.
 */
struct RectSpec {
    Kinesis::AABB2D bounds {};
    glm::vec3 color { 1.F };
    std::string name;
    bool interactive { false };
};

/**
 * @brief Config for generating numeric tick labels on one plot edge.
 */
struct TickLabelsSpec {
    Kinesis::AABB2D plot_bounds {};
    AxisRange range {};
    uint32_t count { 2 };
    TickEdge edge { TickEdge::Bottom };
    glm::vec4 color { 0.65F, 0.65F, 0.65F, 1.F };
    uint8_t decimal_places { 2 };

    /**
     * @brief NDC height of each label strip for Bottom/Top ticks.
     */
    float label_h { 0.055F };

    /**
     * @brief NDC width of each label strip for Left/Right ticks.
     */
    float label_w { 0.12F };

    std::string name_prefix { "tick" };
};

/**
 * @brief One legend row.
 */
struct LegendEntry {
    std::string label;
    glm::vec3 color { 1.F };
};

/**
 * @brief Construction-free vertical legend configuration.
 */
struct LegendSpec {
    glm::vec2 origin { 0.F };
    std::vector<LegendEntry> entries;

    float row_h { 0.07F };
    float swatch_w { 0.04F };
    float gap { 0.012F };
    float text_w { 0.35F };

    glm::vec4 text_color { 0.85F, 0.85F, 0.85F, 1.F };

    std::string name_prefix { "legend" };
    bool interactive { false };
};

/**
 * @brief Expanded construction-free legend layout.
 *
 * Forma should build one rectangle Element per swatch and one text Element
 * per label, then relate all produced elements to the plot root/data id.
 */
struct LegendLayout {
    std::vector<RectSpec> swatches;
    std::vector<LabelSpec> labels;
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
[[nodiscard]] MAYAFLUX_API std::vector<std::span<const double>> series_by_role(
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
MAYAFLUX_API void apply_auto_scale(AxisRange& range,
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
[[nodiscard]] MAYAFLUX_API glm::vec3 palette_color(const std::vector<glm::vec3>& palette,
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
[[nodiscard]] MAYAFLUX_API GeometryFn<float> background(
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
[[nodiscard]] MAYAFLUX_API GeometryFn<float> plot_cursor(
    Kinesis::AABB2D bounds,
    bool vertical = true,
    glm::vec3 color = glm::vec3(0.75F),
    float thickness = 1.F);

// =============================================================================
// Label / tick / legend spec helpers
// =============================================================================

/**
 * @brief Create a single construction-free label spec.
 */
[[nodiscard]] MAYAFLUX_API LabelSpec plot_label(
    std::string text,
    Kinesis::AABB2D bounds,
    glm::vec4 color = { 0.85F, 0.85F, 0.85F, 1.F },
    std::string name = {});

/**
 * @brief Generate construction-free tick label specs.
 *
 * Values are evenly interpolated across spec.range. Label bounds are placed
 * adjacent to spec.plot_bounds according to spec.edge.
 */
[[nodiscard]] MAYAFLUX_API std::vector<LabelSpec> plot_tick_labels(
    const TickLabelsSpec& spec);

/**
 * @brief Convenience overload for generating tick label specs directly.
 */
[[nodiscard]] MAYAFLUX_API std::vector<LabelSpec> plot_tick_labels(
    Kinesis::AABB2D bounds,
    const AxisRange& range,
    uint32_t count,
    TickEdge edge = TickEdge::Bottom,
    glm::vec4 color = { 0.65F, 0.65F, 0.65F, 1.F },
    uint8_t decimal_places = 2,
    float label_h = 0.055F,
    float label_w = 0.12F);

/**
 * @brief Create a construction-free legend spec from labels and colors.
 */
[[nodiscard]] MAYAFLUX_API LegendSpec plot_legend(
    glm::vec2 origin,
    std::span<const std::string> labels,
    std::span<const glm::vec3> colors,
    float row_h = 0.07F,
    float swatch_w = 0.04F,
    glm::vec4 text_color = { 0.85F, 0.85F, 0.85F, 1.F });

/**
 * @brief Expand a legend spec into swatch rectangle specs and text label specs.
 */
[[nodiscard]] MAYAFLUX_API LegendLayout layout_legend(
    const LegendSpec& spec);

} // namespace MayaFlux::Portal::Forma::Plot
