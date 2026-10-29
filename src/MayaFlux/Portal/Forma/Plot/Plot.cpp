#include "Plot.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Portal/Forma/Plot/SeriesBuilder.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma::Plot {

// =============================================================================
// place_label
// =============================================================================

uint32_t place_label(
    Surface& surface,
    FormaTarget buf,
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
        surface.ctx().on_resize(id,
            [surface, id, text = spec.text, color = spec.color, bounds = spec.bounds](uint32_t, uint32_t) mutable {
                const auto dims = Kinesis::ndc_size_to_pixels(
                    { bounds.width(), bounds.height() },
                    surface.window()->get_state().current_width,
                    surface.window()->get_state().current_height);

                if (auto* element = surface.layer().get(id))
                    element->set_text(text, Portal::Text::PressParams { .color = color, .render_bounds = dims });
            });
    }

    return id;
}

// =============================================================================
// place_rect
// =============================================================================

uint32_t place_rect(
    Surface& surface,
    FormaTarget buf,
    const RectSpec& spec,
    uint32_t relate_to)
{
    const auto verts = Kinesis::filled_rect(spec.bounds, glm::vec4(spec.color, 1.F));
    buf.submit(verts);

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
    FormaTarget buf,
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
