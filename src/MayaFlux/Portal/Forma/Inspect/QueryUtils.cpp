#include "QueryUtils.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    void write_row_quad(
        std::vector<uint8_t>& bytes,
        float x_min, float x_max, float bot, float top,
        glm::vec3 bg)
    {
        const uint32_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;

        const glm::vec3 bl { x_min, bot, 0.F };
        const glm::vec3 br { x_max, bot, 0.F };
        const glm::vec3 tl { x_min, top, 0.F };
        const glm::vec3 tr { x_max, top, 0.F };
        constexpr glm::vec3 white { 1.F, 1.F, 1.F };

        auto write = [&](size_t i, glm::vec3 p, glm::vec3 c, float w, glm::vec2 uv) {
            auto* v = bytes.data() + i * stride;
            std::memcpy(v, &p, 12);
            std::memcpy(v + 12, &c, 12);
            std::memcpy(v + 24, &w, 4);
            std::memcpy(v + 28, &uv, 8);
        };

        write(0, bl, bg, 0.F, {});
        write(1, br, bg, 0.F, {});
        write(2, tl, bg, 0.F, {});
        write(3, br, bg, 0.F, {});
        write(4, tr, bg, 0.F, {});
        write(5, tl, bg, 0.F, {});

        write(6, bl, white, 1.F, { 0.F, 1.F });
        write(7, br, white, 1.F, { 1.F, 1.F });
        write(8, tl, white, 1.F, { 0.F, 0.F });
        write(9, br, white, 1.F, { 1.F, 1.F });
        write(10, tr, white, 1.F, { 1.F, 0.F });
        write(11, tl, white, 1.F, { 0.F, 0.F });
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

ValueRow make_value_row(
    const ValueSpec& spec,
    RowBuffer row_buf,
    Layer& layer,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg)
{
    const float top = cursor.y();
    const float bot = top - row_h;
    cursor.advance(row_h);

    const uint32_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
    std::vector<uint8_t> bytes(static_cast<size_t>(12) * stride, 0);
    write_row_quad(bytes, x_min, x_max, bot, top, bg);
    row_buf.buf->submit(bytes);

    auto staging = Buffers::create_image_staging_buffer(
        row_buf.text_image->get_size_bytes());

    Portal::Text::PressParams params {
        .color = { 1.F, 1.F, 1.F, 1.F },
    };

    Element el;
    el.buffer = row_buf.buf;
    el.bounds_hint = Kinesis::AABB2D { .min = { x_min, bot }, .max = { x_max, top } };
    el.interactive = false;
    el.name = spec.label;
    const uint32_t id = layer.add(std::move(el));

    Link link(
        [] { },
        [reader = spec.reader,
            label = spec.label,
            text_image = row_buf.text_image,
            buf = row_buf.buf,
            staging,
            params]() mutable {
            if (!buf || !reader)
                return;
            Portal::Text::repress(text_image,
                label + ": " + reader(),
                params, staging);
            buf->bind_texture(0, text_image);
        });

    return ValueRow {
        .element_id = id,
        .buf = std::move(row_buf.buf),
        .text = std::move(row_buf.text_image),
        .link = std::move(link),
    };
}

ValueGroup make_value_group(
    std::span<const ValueSpec> values,
    RowBuffer header_buf,
    std::span<const RowBuffer> row_bufs,
    Layer& layer,
    Context& context,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open)
{
    auto header = Collapsible {}
                      .initially_open(initially_open)
                      .closed_color(glm::vec3(0.25F))
                      .open_color(glm::vec3(0.35F))
                      .label(header_buf.text_image)
                      .place(std::move(header_buf.buf), layer, context, cursor, x_min, x_max, row_h);

    std::vector<ValueRow> rows;
    rows.reserve(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        auto row = make_value_row(values[i], row_bufs[i], layer, cursor, x_min, x_max, row_h);
        header.attach(layer, row.element_id);
        rows.push_back(std::move(row));
    }

    header.cursor_out = cursor;

    return ValueGroup {
        .header = std::move(header),
        .rows = std::move(rows),
    };
}

} // namespace MayaFlux::Portal::Forma
