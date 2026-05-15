#include "QueryUtils.hpp"

#include "MayaFlux/Buffers/Forma/FormaBuffer.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

#include "MayaFlux/Portal/Forma/Forma.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"
#include "MayaFlux/Portal/Text/Text.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

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

std::pair<uint32_t, uint32_t> row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h)
{
    const auto& ws = window->get_state();
    const auto w = static_cast<uint32_t>(
        (x_max - x_min) * 0.5F * static_cast<float>(ws.current_width));
    const auto h = static_cast<uint32_t>(
        row_h * 0.5F * static_cast<float>(ws.current_height));

    const uint32_t min_h = Portal::Text::get_default_atlas().line_height();

    return { std::max(w, 1U), std::max(h, min_h) };
}

ValueRow make_value_row(
    const ValueSpec& spec,
    Layer& layer,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    glm::vec3 bg)
{
    const auto [tw, th] = row_pixel_dims(window, x_min, x_max, row_h);

    const std::string initial = spec.label + ": "
        + (spec.reader ? spec.reader() : std::string {});

    Portal::Text::PressParams params {
        .color = { 1.F, 1.F, 1.F, 1.F },
        .budget_h = th,
    };

    auto text_image = Portal::Text::press(initial, { tw, th }, params);
    auto staging = Buffers::create_image_staging_buffer(text_image->get_size_bytes());

    const uint32_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
    auto buf = Portal::Forma::create_buffer(
        window, static_cast<size_t>(12) * stride,
        Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST,
        { { "text", text_image } });

    const float top = cursor.y();
    const float bot = top - row_h;
    cursor.advance(row_h);

    std::vector<uint8_t> bytes(static_cast<size_t>(12) * stride, 0);
    write_row_quad(bytes, x_min, x_max, bot, top, bg);
    buf->submit(bytes);

    Element el;
    el.buffer = buf;
    el.bounds_hint = Kinesis::AABB2D { .min = { x_min, bot }, .max = { x_max, top } };
    el.interactive = false;
    el.name = spec.label;
    const uint32_t id = layer.add(std::move(el));

    Link link(
        [] { },
        [reader = spec.reader,
            label = spec.label,
            text_image,
            buf,
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
        .buf = buf,
        .text = text_image,
        .link = std::move(link),
    };
}

ValueGroup make_value_group(
    std::string_view header_label,
    std::span<const ValueSpec> values,
    Layer& layer,
    Context& context,
    const std::shared_ptr<Core::Window>& window,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h,
    bool initially_open)
{
    const auto [tw, th] = row_pixel_dims(window, x_min, x_max, row_h);

    auto header_text = Portal::Text::press(
        header_label,
        { tw, th },
        Portal::Text::PressParams {
            .color = { 1.F, 1.F, 1.F, 1.F },
            .budget_h = th,
        });

    auto header = make_collapsible(
        window, layer, context, get_bridge(), cursor,
        x_min, x_max, row_h, initially_open,
        glm::vec3(0.25F), glm::vec3(0.35F),
        header_text);

    std::vector<ValueRow> rows;
    rows.reserve(values.size());
    for (const auto& spec : values) {
        auto row = make_value_row(spec, layer, window, cursor,
            x_min, x_max, row_h);
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
