#include "Collapsible.hpp"
#include "Entry.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

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

} // namespace

// =============================================================================
// Collapsible
// =============================================================================

Collapsible& Collapsible::place(
    FormaTarget in_buf,
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
        const glm::uvec2 dims = row_pixel_dims(surface.window(), x_min, x_max, row_h);

        auto params = std::make_shared<Portal::Text::PressParams>(Portal::Text::PressParams {
            .color = m_label_color,
            .background = { m_initially_open ? m_color_open : m_color_closed, 1.F },
            .render_bounds = dims,
        });

        Element el;
        el.buffer = buf;
        el.bounds_hint = row_rect;
        el.with_text(m_label_text, *params, row_rect);

        hid = surface.layer().add(el);
        open_state->id = hid;

        auto repress = [el, surface, params, label_text = m_label_text,
                           open = open_state, color_closed = m_color_closed,
                           color_open = m_color_open, x_min, x_max, row_h]() mutable {
            params->render_bounds = row_pixel_dims(surface.window(), x_min, x_max, row_h);
            params->background = { open->value ? color_open : color_closed, 1.F };
            el.set_text(label_text, *params);
        };

        surface.ctx().on_press(hid, IO::MouseButtons::Left,
            [open = open_state, surface, hid, repress](uint32_t, glm::vec2) mutable {
                const bool next = !open->value;
                open->write(next);
                for (auto rel_id : surface.layer().related_ids(hid))
                    surface.layer().set_visible(rel_id, next);
                repress();
            });

        surface.ctx().on_resize(hid,
            [repress](uint32_t, uint32_t) mutable { repress(); });
    }

    header_bounds = row_rect;
    header_id = hid;
    open = open_state;
    cursor_out = cursor;
    return *this;
}

Collapsible& Collapsible::place(
    FormaTarget in_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float row_h)
{
    return place(std::move(in_buf), surface, cursor,
        cursor.x_min(), cursor.x_max(), row_h);
}

void Collapsible::attach(Layer& layer, uint32_t body_id) const
{
    if (!open)
        return;

    layer.relate(header_id, body_id);
    layer.set_visible(body_id, open->value);
}

// =============================================================================
// Free function escape hatch
// =============================================================================

Collapsible make_collapsible(
    FormaTarget buf,
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
