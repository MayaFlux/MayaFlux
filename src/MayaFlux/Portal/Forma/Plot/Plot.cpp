#include "Plot.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Portal/Forma/Plot/SeriesBuilder.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma::Plot {

namespace {

    /// @brief Headroom on a label's staging buffer so a later resize within
    ///        this margin reuses it instead of falling back to an internal
    ///        one-shot allocation for that call.
    constexpr size_t k_staging_margin = 2;

} // namespace

// =============================================================================
// place_label
// =============================================================================

uint32_t place_label(
    Surface& surface,
    std::shared_ptr<Buffers::FormaBuffer> buf,
    const LabelSpec& spec,
    uint32_t relate_to)
{
    const bool auto_render_bounds = spec.render_bounds.x <= 0 || spec.render_bounds.y <= 0;

    const auto render_bounds = auto_render_bounds
        ? Kinesis::ndc_size_to_pixels(
              { spec.bounds.width(), spec.bounds.height() },
              surface.window()->get_state().current_width,
              surface.window()->get_state().current_height)
        : spec.render_bounds;

    Element el;
    el.with_buffer(buf)
        .with_bounds(spec.bounds)
        .with_name(spec.name)
        .with_text(spec.text,
            Portal::Text::PressParams {
                .color = spec.color,
                .render_bounds = render_bounds,
            },
            spec.bounds);

    if (!spec.interactive)
        el.non_interactive();

    const uint32_t id = surface.layer().add(std::move(el));
    if (relate_to != 0)
        surface.layer().relate(relate_to, id);

    if (auto_render_bounds) {
        auto staging = Buffers::create_image_staging_buffer(
            static_cast<size_t>(render_bounds.x) * render_bounds.y * 4 * k_staging_margin);

        surface.ctx().on_resize(id,
            [buf, surface, id, text = spec.text, color = spec.color, bounds = spec.bounds,
                staging](uint32_t, uint32_t) mutable {
                const auto dims = Kinesis::ndc_size_to_pixels(
                    { bounds.width(), bounds.height() },
                    surface.window()->get_state().current_width,
                    surface.window()->get_state().current_height);

                const size_t required_bytes = static_cast<size_t>(dims.x) * dims.y * 4;

                if (required_bytes > staging->get_size_bytes()) {
                    staging->resize(required_bytes * 2, false);
                }

                auto image = Portal::Text::press(
                    text, dims, { .color = color }, staging);
                if (auto* element = surface.layer().get(id))
                    element->texture = image;
                buf->bind_texture(0, image);
            });
    }

    return id;
}

// =============================================================================
// place_rect
// =============================================================================

uint32_t place_rect(
    Surface& surface,
    std::shared_ptr<Buffers::FormaBuffer> buf,
    const RectSpec& spec,
    uint32_t relate_to)
{
    const auto verts = Kinesis::filled_rect(spec.bounds, spec.color);
    buf->submit(verts);

    Element el;
    el.with_buffer(std::move(buf))
        .with_bounds(spec.bounds)
        .with_name(spec.name);

    if (!spec.interactive)
        el.non_interactive();

    const uint32_t id = surface.layer().add(std::move(el));
    if (relate_to != 0)
        surface.layer().relate(relate_to, id);

    return id;
}

// =============================================================================
// place
// =============================================================================

Mapped<std::shared_ptr<Kakshya::PlotContainer>> place(
    Surface& surface,
    std::shared_ptr<Buffers::FormaBuffer> buf,
    SeriesSpec spec,
    std::shared_ptr<Kakshya::PlotContainer> container)
{
    auto mapped = make_mapped<std::shared_ptr<Kakshya::PlotContainer>>(
        std::move(container), std::move(spec.fn), std::move(buf));

    mapped.element.id = surface.layer().add(mapped.element);
    mapped.state->id = mapped.element.id;
    mapped.force_redraw_on_sync = true;
    mapped.sync(&surface.layer());

    return mapped;
}

} // namespace MayaFlux::Portal::Forma::Plot
