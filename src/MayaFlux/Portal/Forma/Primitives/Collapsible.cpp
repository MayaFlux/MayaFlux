#include "Collapsible.hpp"
#include "Entry.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief Headroom on the label's staging buffer so a later resize
    ///        within this margin reuses it instead of falling back to an
    ///        internal one-shot allocation for that call.
    constexpr size_t k_staging_margin = 2;

    [[nodiscard]] GeometryFn<bool> collapsible_header_geom(
        float y_top,
        float x_min,
        float x_max,
        float row_h,
        glm::vec3 color_closed,
        glm::vec3 color_open)
    {
        return [y_top, x_min, x_max, row_h, color_closed, color_open](
                   bool open, std::vector<uint8_t>& out, Element& el) {
            const float bot = y_top - row_h;
            const glm::vec3 col = open ? color_open : color_closed;

            const size_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
            out.assign(static_cast<size_t>(6) * stride, 0);

            const glm::vec3 bl { x_min, bot, 0.F };
            const glm::vec3 br { x_max, bot, 0.F };
            const glm::vec3 tl { x_min, y_top, 0.F };
            const glm::vec3 tr { x_max, y_top, 0.F };

            auto write = [&](size_t idx, glm::vec3 pos) {
                auto* v = out.data() + idx * stride;
                std::memcpy(v, &pos, 12);
                std::memcpy(v + 12, &col, 12);
            };

            write(0, bl);
            write(1, br);
            write(2, tl);
            write(3, br);
            write(4, tr);
            write(5, tl);

            el.bounds_hint = Kinesis::AABB2D {
                .min = { x_min, bot },
                .max = { x_max, y_top },
            };
        };
    }

    /// @brief A fully-textured TRIANGLE_LIST quad, weight=1 so forma_multi.frag
    ///        samples textures[0]. Matches Entry.cpp's own helper of the same
    ///        shape; not shared across files, same as Element.cpp's private
    ///        textured_mesh_rect isn't either.
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

// =============================================================================
// Collapsible
// =============================================================================

Collapsible& Collapsible::place(
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    buf = std::move(in_buf);

    const Kinesis::AABB2D advanced = cursor.advance(row_h);
    const float y_top = advanced.max.y;
    const Kinesis::AABB2D row_rect { .min = { x_min, advanced.min.y }, .max = { x_max, y_top } };

    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(m_initially_open);

    uint32_t hid = 0;

    if (m_label_text.empty()) {
        Mapped<bool> mapped;
        mapped.state = open_state;
        mapped.geometry_fn = collapsible_header_geom(y_top, x_min, x_max, row_h, m_color_closed, m_color_open);
        mapped.element.buffer = buf;
        mapped.element.bounds_hint = row_rect;

        hid = surface.layer().add(mapped.element);
        mapped.element.id = hid;
        open_state->id = hid;
        mapped.sync();

        surface.ctx().on_press(hid, IO::MouseButtons::Left,
            [m = std::move(mapped), open = open_state, surface, hid](uint32_t, glm::vec2) mutable {
                const bool next = !open->value;
                open->write(next);
                for (auto rel_id : surface.layer().related_ids(hid))
                    surface.layer().set_visible(rel_id, next);
                m.sync();
            });
    } else {
        buf->submit(textured_quad(row_rect));

        const glm::uvec2 dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
        auto staging = Buffers::create_image_staging_buffer(
            static_cast<size_t>(dims.x) * dims.y * 4 * k_staging_margin);

        auto image_slot = Portal::Text::press(m_label_text, dims,
            { .color = m_label_color,
                .background = { m_initially_open ? m_color_open : m_color_closed, 1.F } },
            staging);
        buf->bind_texture(0, image_slot);

        Element el;
        el.buffer = buf;
        el.bounds_hint = row_rect;

        hid = surface.layer().add(el);
        open_state->id = hid;

        const std::string label_text = m_label_text;
        const glm::vec4 label_color = m_label_color;
        const glm::vec3 color_closed = m_color_closed;
        const glm::vec3 color_open = m_color_open;

        surface.ctx().on_press(hid, IO::MouseButtons::Left,
            [buf = buf, open = open_state, surface, hid, staging,
                label_text, label_color, color_closed, color_open](uint32_t, glm::vec2) mutable {
                const bool next = !open->value;
                open->write(next);
                for (auto rel_id : surface.layer().related_ids(hid))
                    surface.layer().set_visible(rel_id, next);
            });

        surface.ctx().on_resize(hid,
            [buf = buf, surface, staging, open = open_state,
                label_text, label_color, color_closed, color_open,
                x_min, x_max, row_h](uint32_t, uint32_t) {
                const glm::uvec2 new_dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);
                const size_t required_bytes = static_cast<size_t>(new_dims.x) * new_dims.y * 4;

                if (required_bytes > staging->get_size_bytes()) {
                    staging->resize(required_bytes * k_staging_margin, false);
                }

                auto image = Portal::Text::press(
                    label_text, new_dims,
                    { .color = label_color,
                        .background = { open->value ? color_open : color_closed, 1.F } },
                    staging);
                buf->bind_texture(0, image);
            });
    }

    header_bounds = row_rect;
    header_id = hid;
    open = open_state;
    cursor_out = cursor;
    return *this;
}

Collapsible& Collapsible::place(
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h)
{
    return place(std::move(in_buf), surface, cursor,
        cursor.x_min(), cursor.x_max(), row_h);
}

void Collapsible::attach(Layer& layer, uint32_t body_id) const
{
    layer.relate(header_id, body_id);
    layer.set_visible(body_id, open->value);
}

// =============================================================================
// Free function escape hatch
// =============================================================================

Collapsible make_collapsible(
    std::shared_ptr<Buffers::FormaBuffer> buf,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min,
    float x_max,
    float row_h,
    bool initially_open,
    glm::vec3 color_closed,
    glm::vec3 color_open,
    std::string label)
{
    return Collapsible {}
        .initially_open(initially_open)
        .closed_color(color_closed)
        .open_color(color_open)
        .label(std::move(label))
        .place(std::move(buf), surface, cursor, x_min, x_max, row_h);
}

} // namespace MayaFlux::Portal::Forma
