#pragma once

#include "PlotSource.hpp"
#include "PlotSpec.hpp"
#include "SeriesBuilder.hpp"

namespace MayaFlux::Portal::Forma {
class Surface;
}

namespace MayaFlux::Portal::Forma::Plot {

/**
 * @brief Place a plot element onto a Surface using a FormaTarget.
 *
 * Constructs a Mapped<shared_ptr<PlotContainer>> from the geometry function
 * carried by @p spec, registers the element on @p surface's layer, and runs
 * one initial sync so the first frame has valid geometry. Sets
 * force_redraw_on_sync so geometry regenerates on every sync() call when
 * the container pointer is stable but its data changes.
 *
 * Buffer construction and scheduling are the caller's responsibility.
 * Create a batch with surface.render_config(), then pass a slice with
 * spec.topology. Other plot geometry can use slices of that batch.
 *
 * @param surface    Surface to register the element on.
 * @param buf        Target the series draws into, a slice of spec.topology.
 * @param spec       Result of a SeriesBuilder terminal. Carries the geometry
 *                   function and input topology for this encoding.
 *                   Raw GeometryFn callers are responsible for calling
 *                   container->process_default() inside their function;
 *                   SeriesBuilder terminals call it internally.
 * @param container  PlotContainer with series bound and marked ready for
 *                   processing. Becomes the initial MappedState value.
 * @return Fully constructed Mapped with element registered on the surface layer.
 *         Call sync() each frame to drive geometry updates.
 */
[[nodiscard]] MAYAFLUX_API
    Mapped<std::shared_ptr<Kakshya::PlotContainer>>
    place(
        Surface& surface,
        FormaTarget buf,
        SeriesSpec spec,
        std::shared_ptr<Kakshya::PlotContainer> container);

/**
 * @brief Place a text label element onto @p surface using a text-capable target.
 *
 * @p buf must have been created with an additional_textures slot named "text"
 * (or equivalent). Buffer construction is the caller's responsibility;
 * Portal::Forma::plot() handles this automatically.
 *
 * @param surface    Target surface.
 * @param buf        TRIANGLE_LIST slice of a text-capable buffer. Labels may
 *                   share that buffer.
 * @param spec       Label geometry and style.
 * @param relate_to  If non-zero, the produced element is related to this id.
 * @return           Element id of the produced label element.
 */
[[nodiscard]] MAYAFLUX_API uint32_t place_label(
    Surface& surface,
    FormaTarget buf,
    const LabelSpec& spec,
    uint32_t relate_to = 0);

/**
 * @brief Place a filled rectangle element onto @p surface using a target.
 *
 * @p buf should be a TRIANGLE_STRIP slice of the plot batch. A plot batch
 * may also contain text slices. Buffer construction is the caller's responsibility.
 *
 * @param surface    Target surface.
 * @param buf        Target for the four rectangle vertices.
 * @param spec       Rectangle geometry and color.
 * @param relate_to  If non-zero, the produced element is related to this id.
 * @return           Element id of the produced rect element.
 */
[[nodiscard]] MAYAFLUX_API uint32_t place_rect(
    Surface& surface,
    FormaTarget buf,
    const RectSpec& spec,
    uint32_t relate_to = 0);

/**
 * @brief Begin a Series chain.
 *
 * Convenience constructor for GeometryFn<shared_ptr<PlotContainer>>.
 * Illustrative, not idiomatic. The raw GeometryFn lambda is always the
 * primary path and is preferred when the builder cannot express what you need.
 *
 * @code
 * // 7 SPATIAL_Y series -> 7 waveforms, blue palette
 * auto geom = Plot::series()
 *     .y(Role::SPATIAL_Y, AxisRange{}.auto_scale(), { 0.2F, 0.8F, 1.0F })
 *     .as_waveform()
 *     .thickness(1.5F)
 *     .done();
 *
 * // Lissajous scatter, X and Y each with their own range
 * auto geom = Plot::series()
 *     .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))
 *     .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { 0.9F, 0.3F, 0.8F })
 *     .as_scatter()
 *     .point_size(3.F)
 *     .done();
 * @endcode
 */
[[nodiscard]] inline Series series()
{
    return Series {};
}

/**
 * @brief Begin a Source chain.
 *
 * Constructs a PlotContainer incrementally via .as().from() pairs.
 * The raw PlotContainer API remains available after .build() for cases
 * the builder cannot express.
 *
 * @code
 * auto container = Plot::source()
 *     .as("fm_sine", 512, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(sine)
 *     .as("mod",     512, Role::SPATIAL_Y, DataModality::AUDIO_1D).from(mod)
 *     .build();
 * @endcode
 */
[[nodiscard]] inline Source source()
{
    return Source {};
}

} // namespace MayaFlux::Portal::Forma::Plot
