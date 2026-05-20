#pragma once

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"

#include "AxisRange.hpp"
namespace MayaFlux::Portal::Forma {
class Surface;
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

/**
 * @brief Place a plot element onto a Surface using a pre-built FormaBuffer.
 *
 * Constructs a Mapped<shared_ptr<PlotContainer>> from the geometry function
 * carried by @p spec, registers the element on @p surface's layer, and runs
 * one initial sync so the first frame has valid geometry.
 *
 * Buffer construction and scheduling are the caller's responsibility.
 * Use Portal::Forma::create_buffer(surface.window(), spec.capacity_for(N),
 * spec.topology) to build the buffer, and schedule_metro to drive
 * process_default() + state->write() at the desired rate.
 *
 * @param surface    Surface to register the element on.
 * @param buf        Pre-built, registered FormaBuffer sized for this encoding.
 *                   Use spec.capacity_for(N) and spec.topology to construct it.
 * @param spec       Result of a SeriesBuilder terminal. Carries the geometry
 *                   function, topology, and capacity arithmetic for this encoding.
 * @param container  PlotContainer with series bound and marked ready for
 *                   processing. Becomes the initial MappedState value.
 * @return Fully constructed Mapped with element registered on the surface layer.
 *         Hold the returned Mapped or its state to drive updates.
 */
[[nodiscard]] MAYAFLUX_API
    Mapped<std::shared_ptr<Kakshya::PlotContainer>>
    place(
        Surface& surface,
        std::shared_ptr<Buffers::FormaBuffer> buf,
        SeriesSpec spec,
        std::shared_ptr<Kakshya::PlotContainer> container);

} // namespace MayaFlux::Portal::Forma::Plot
