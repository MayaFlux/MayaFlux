#include "Plot.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Portal/Forma/Plot/SeriesBuilder.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma::Plot {

namespace {

    glm::uvec2 bounds_to_pixels(
        const std::shared_ptr<Core::Window>& window,
        Kinesis::AABB2D bounds)
    {
        const auto& ws = window->get_state();
        const auto w = static_cast<uint32_t>(
            std::max(bounds.width(), 0.001F)
            * 0.5F
            * static_cast<float>(ws.current_width));
        const auto h = static_cast<uint32_t>(
            std::max(bounds.height(), 0.001F)
            * 0.5F
            * static_cast<float>(ws.current_height));
        return { std::max(w, 1U), std::max(h, 1U) };
    }

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
    const auto render_bounds = spec.render_bounds.x > 0 && spec.render_bounds.y > 0
        ? spec.render_bounds
        : bounds_to_pixels(surface.window(), spec.bounds);

    Element el;
    el.with_buffer(std::move(buf))
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
    mapped.sync();

    return mapped;
}

} // namespace MayaFlux::Portal::Forma::Plot
