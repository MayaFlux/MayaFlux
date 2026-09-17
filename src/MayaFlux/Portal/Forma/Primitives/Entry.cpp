#include "Entry.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief A fully-textured TRIANGLE_LIST quad, weight=1 so forma_multi.frag
    ///        samples textures[0]. The row's background now lives in that
    ///        texture's own pixels (Portal::Text::PressParams::background),
    ///        not a separate vertex layer.
    std::array<Kakshya::MeshVertex, 6> textured_quad(Kinesis::AABB2D region)
    {
        using V = Kakshya::MeshVertex;
        const glm::vec2 mn = region.min;
        const glm::vec2 mx = region.max;
        return { {
            V { .position = { mn.x, mn.y, 0.F }, .weight = 1.F, .uv = { 0.F, 1.F } },
            V { .position = { mx.x, mn.y, 0.F }, .weight = 1.F, .uv = { 1.F, 1.F } },
            V { .position = { mn.x, mx.y, 0.F }, .weight = 1.F, .uv = { 0.F, 0.F } },
            V { .position = { mx.x, mn.y, 0.F }, .weight = 1.F, .uv = { 1.F, 1.F } },
            V { .position = { mx.x, mx.y, 0.F }, .weight = 1.F, .uv = { 1.F, 0.F } },
            V { .position = { mn.x, mx.y, 0.F }, .weight = 1.F, .uv = { 0.F, 0.F } },
        } };
    }

} // namespace

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

    row_buf.buf->submit(textured_quad(row_rect));

    auto staging = Buffers::create_image_staging_buffer(
        row_buf.text_image->get_size_bytes());

    Portal::Text::PressParams params {
        .color = { 1.F, 1.F, 1.F, 1.F },
        .background = { bg, 1.F },
    };

    Element el;
    el.buffer = row_buf.buf;
    el.bounds_hint = row_rect;
    el.interactive = false;
    el.name = spec.label;
    const uint32_t id = surface.layer().add(el);

    auto image_slot = std::make_shared<std::shared_ptr<Core::VKImage>>(row_buf.text_image);

    auto compose = [reader = spec.reader,
                        label = spec.label,
                        image_slot,
                        buf = row_buf.buf,
                        staging,
                        params,
                        last = std::optional<std::string> {}]() mutable {
        if (!buf || !reader)
            return;

        std::string text = label + ": " + reader();
        if (last && *last == text)
            return;
        last = text;

        Portal::Text::repress(*image_slot, text, params, staging);
        buf->bind_texture(0, *image_slot);
    };

    surface.ctx().on_resize(id,
        [buf = row_buf.buf, surface, image_slot, label = spec.label,
            reader = spec.reader, params, x_min, x_max, row_h](uint32_t, uint32_t) {
            const glm::uvec2 new_dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
            Portal::Text::PressParams resize_params = params;
            resize_params.budget_h = new_dims.y;
            const std::string text = reader ? (label + ": " + reader()) : label;
            *image_slot = Portal::Text::press(text, new_dims, resize_params);
            buf->bind_texture(0, *image_slot);
        });

    Link link(compose, compose);

    return Entry {
        .element_id = id,
        .buf = std::move(row_buf.buf),
        .text = std::move(row_buf.text_image),
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
    std::shared_ptr<Buffers::FormaBuffer> header_buf,
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
    std::shared_ptr<Buffers::FormaBuffer> header_buf,
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
