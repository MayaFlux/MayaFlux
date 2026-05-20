#include "Collapsible.hpp"

#include "MayaFlux/Portal/Forma/Surface.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    [[nodiscard]] GeometryFn<bool> collapsible_header_geom(
        float y_top,
        float x_min,
        float x_max,
        float row_h,
        glm::vec3 color_closed,
        glm::vec3 color_open,
        bool has_label)
    {
        return [y_top, x_min, x_max, row_h, color_closed, color_open, has_label](
                   bool open, std::vector<uint8_t>& out, Element& el) {
            const float bot = y_top - row_h;
            const glm::vec3 col = open ? color_open : color_closed;

            const size_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
            const size_t vertex_count = has_label ? 12 : 6;

            out.assign(vertex_count * stride, 0);

            const glm::vec3 bl { x_min, bot, 0.F };
            const glm::vec3 br { x_max, bot, 0.F };
            const glm::vec3 tl { x_min, y_top, 0.F };
            const glm::vec3 tr { x_max, y_top, 0.F };
            constexpr glm::vec3 white { 1.F, 1.F, 1.F };

            auto write = [&](size_t idx, glm::vec3 pos, glm::vec3 c, float w, glm::vec2 uv) {
                auto* v = out.data() + idx * stride;
                std::memcpy(v, &pos, 12);
                std::memcpy(v + 12, &c, 12);
                std::memcpy(v + 24, &w, 4);
                std::memcpy(v + 28, &uv, 8);
            };

            write(0, bl, col, 0.F, {});
            write(1, br, col, 0.F, {});
            write(2, tl, col, 0.F, {});
            write(3, br, col, 0.F, {});
            write(4, tr, col, 0.F, {});
            write(5, tl, col, 0.F, {});

            if (has_label) {
                write(6, bl, white, 1.F, { 0.F, 1.F });
                write(7, br, white, 1.F, { 1.F, 1.F });
                write(8, tl, white, 1.F, { 0.F, 0.F });
                write(9, br, white, 1.F, { 1.F, 1.F });
                write(10, tr, white, 1.F, { 1.F, 0.F });
                write(11, tl, white, 1.F, { 0.F, 0.F });
            }

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
    std::shared_ptr<Buffers::FormaBuffer> in_buf,
    Surface& surface,
    LayoutCursor& cursor,
    float x_min, float x_max, float row_h)
{
    buf = std::move(in_buf);

    const float y_top = cursor.y();
    cursor.advance(row_h);

    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(m_initially_open);

    Mapped<bool> mapped;
    mapped.state = open_state;
    mapped.geometry_fn = collapsible_header_geom(
        y_top, x_min, x_max, row_h, m_color_closed, m_color_open, m_label != nullptr);
    mapped.element.buffer = buf;
    mapped.element.bounds_hint = Kinesis::AABB2D {
        .min = { x_min, cursor.y() },
        .max = { x_max, y_top },
    };

    const uint32_t hid = surface.layer().add(mapped.element);
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

    header_id = hid;
    open = open_state;
    cursor_out = cursor;
    return *this;
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
    glm::vec3 color_open)
{
    return Collapsible {}
        .initially_open(initially_open)
        .closed_color(color_closed)
        .open_color(color_open)
        .place(std::move(buf), surface, cursor, x_min, x_max, row_h);
}

} // namespace MayaFlux::Portal::Forma
