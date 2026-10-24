#include "Entry.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

namespace MayaFlux::Portal::Forma {

glm::uvec2 row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h)
{
    const auto& ws = window->get_state();
    const auto w = static_cast<uint32_t>(
        (x_max - x_min) * 0.5F * static_cast<float>(ws.current_width));
    const auto h = static_cast<uint32_t>(
        row_h * 0.5F * static_cast<float>(ws.current_height));
    auto atlas = Text::TypeFaceFoundry::instance().get_default_glyph_atlas();
    if (!atlas) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::Runtime,
            std::source_location::current(),
            "Failed to get default glyph atlas for row pixel dimension calculation");
    }
    const uint32_t min_h = atlas->pixel_size();

    return { std::max(w, 1U), std::max(h, min_h) };
}

Entry make_entry(
    const EntrySpec& spec,
    EntryBuffer row_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg)
{
    const Kinesis::AABB2D advanced = cursor.advance(row_h);
    const Kinesis::AABB2D row_rect { .min = { x_min, advanced.min.y }, .max = { x_max, advanced.max.y } };

    const glm::uvec2 dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);

    auto params = std::make_shared<Portal::Text::PressParams>(Portal::Text::PressParams {
        .color = { 1.F, 1.F, 1.F, 1.F },
        .background = { bg, 1.F },
        .render_bounds = dims,
    });

    Element el;
    el.buffer = row_buf.buf;
    el.bounds_hint = row_rect;
    el.interactive = false;
    el.name = spec.label;
    el.with_text(spec.label, *params, row_rect);
    const uint32_t id = surface.layer().add(el);

    auto compose = [reader = spec.reader,
                       label = spec.label,
                       el,
                       params,
                       last = std::optional<std::string> {}]() mutable {
        if (!reader)
            return;

        std::string text = label + ": " + reader();
        if (last && *last == text)
            return;
        last = text;

        el.set_text(text, *params);
    };

    surface.ctx().on_resize(id,
        [el, surface, params, label = spec.label, reader = spec.reader,
            x_min, x_max, row_h](uint32_t, uint32_t) mutable {
            params->render_bounds = row_pixel_dims(surface.window(), x_min, x_max, row_h);
            el.set_text(reader ? (label + ": " + reader()) : label, *params);
        });

    Link link(compose, compose);

    return Entry {
        .element_id = id,
        .buf = std::move(row_buf.buf),
        .link = std::move(link),
        .row_bounds = row_rect,
    };
}

Entry make_entry(
    const EntrySpec& spec,
    EntryBuffer row_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h,
    glm::vec3 bg)
{
    return make_entry(spec, std::move(row_buf), surface, cursor,
        cursor.x_min(), cursor.x_max(), row_h, bg);
}

EntryGroup make_entry_group(
    std::span<const EntrySpec> entrys,
    std::string_view header_label,
    FormaTarget header_buf,
    std::span<const EntryBuffer> row_bufs,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open)
{
    auto header = Collapsible {}
                      .initially_open(initially_open)
                      .closed_color(glm::vec3(0.25F))
                      .open_color(glm::vec3(0.35F))
                      .label(std::string(header_label))
                      .place(std::move(header_buf), surface, cursor, x_min, x_max, row_h);

    std::vector<Entry> rows;
    rows.reserve(entrys.size());
    for (size_t i = 0; i < entrys.size(); ++i) {
        auto row = make_entry(entrys[i], row_bufs[i], surface, cursor, x_min, x_max, row_h);
        header.attach(surface.layer(), row.element_id);
        rows.push_back(std::move(row));
    }

    header.cursor_out = cursor;

    return EntryGroup {
        .header = std::move(header),
        .rows = std::move(rows),
    };
}

EntryGroup make_entry_group(
    std::span<const EntrySpec> entrys,
    std::string_view header_label,
    FormaTarget header_buf,
    std::span<const EntryBuffer> row_bufs,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h,
    bool initially_open)
{
    return make_entry_group(entrys, header_label, std::move(header_buf), row_bufs,
        surface, cursor, cursor.x_min(), cursor.x_max(), row_h, initially_open);
}

} // namespace MayaFlux::Portal::Forma
