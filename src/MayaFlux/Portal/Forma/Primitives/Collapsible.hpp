#pragma once

namespace MayaFlux::Core {
class VKImage;
}

#include "LayoutCursor.hpp"

#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"
#include "MayaFlux/Portal/Forma/Bridge.hpp"
#include "MayaFlux/Portal/Forma/Context.hpp"
#include "MayaFlux/Portal/Forma/Forma.hpp"
#include "MayaFlux/Portal/Forma/Layer.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief Result of make_collapsible().
 *
 * Owns all Forma constructs produced for one collapsible header. The header
 * element is registered in the layer; any body elements the caller adds via
 * layer.relate(header_id, body_id) will be shown and hidden automatically
 * when the open state changes. cursor_out is positioned at the bottom of
 * the header strip, ready for the first body element.
 */
struct Collapsible {
    /// @brief Element id of the header strip. Pass to layer.relate() and ctx.on_press().
    uint32_t header_id {};

    /// @brief Open/closed state. Write to toggle; the sync loop picks up changes.
    std::shared_ptr<MappedState<bool>> open;

    /// @brief The FormaBuffer backing the header geometry.
    std::shared_ptr<Buffers::FormaBuffer> buf;

    /// @brief Cursor positioned at the bottom edge of the header, ready for body elements.
    LayoutCursor cursor_out;

    /**
     * @brief Relate a body element to this collapsible and sync its visibility
     *        to the current open state.
     *
     * Replaces a manual layer.relate(header_id, body_id) call. Ensures the
     * body's render state matches @ref open at attach time, so callers do not
     * need to track the initial toggle state.
     *
     * @param layer   Layer the body element lives on.
     * @param body_id Element id returned from layer.add(...).
     */
    void attach(Layer& layer, uint32_t body_id) const
    {
        layer.relate(header_id, body_id);
        layer.set_visible(body_id, open->value);
    }
};

/**
 * @brief Geometry function for a collapsible header strip.
 *
 * When @p label is null: emits 6 vertices (background quad only, weight=0).
 * When @p label is non-null: emits 12 vertices — background quad (weight=0)
 * followed by a full-cover overlay quad (weight=1, white, UVs [0,1]) that
 * samples the label image via the multi-texture shader.
 *
 * Vertex layout matches VertexLayout::for_meshes() (stride 60):
 *   offset  0  position  vec3
 *   offset 12  color     vec3
 *   offset 24  weight    float
 *   offset 28  uv        vec2
 *
 * @param cursor_in    Shared cursor state. top = cursor_in->value + row_h,
 *                     bot = cursor_in->value (post-advance convention).
 * @param x_min        Left edge in NDC.
 * @param x_max        Right edge in NDC.
 * @param row_h        Strip height in NDC units.
 * @param color_closed Background color when closed.
 * @param color_open   Background color when open.
 * @param label        Optional GPU image for the label overlay. nullptr = no overlay.
 */
[[nodiscard]] inline GeometryFn<bool> collapsible_header_geom(
    const std::shared_ptr<MappedState<float>>& cursor_in,
    float x_min,
    float x_max,
    float row_h,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F),
    const std::shared_ptr<Core::VKImage>& label = nullptr)
{
    const bool has_label = label != nullptr;

    return [cursor_in, x_min, x_max, row_h, color_closed, color_open, has_label](
               bool open, std::vector<uint8_t>& out, Element& el) {
        const float top = cursor_in->value + row_h;
        const float bot = cursor_in->value;
        const glm::vec3 col = open ? color_open : color_closed;

        const size_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
        const size_t vertex_count = has_label ? 12 : 6;

        out.assign(vertex_count * stride, 0);

        const glm::vec3 bl { x_min, bot, 0.F };
        const glm::vec3 br { x_max, bot, 0.F };
        const glm::vec3 tl { x_min, top, 0.F };
        const glm::vec3 tr { x_max, top, 0.F };
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
            .max = { x_max, top },
        };
    };
}

/**
 * @brief Construct a collapsible header strip.
 *
 * Creates and registers a FormaBuffer, constructs a Mapped<bool> whose
 * geometry function produces the header quad, registers it with the Bridge
 * sync loop, and wires the on_press toggle via @p ctx. Caller adds body
 * elements to @p layer and calls layer.relate(col.header_id, body_id) for
 * each one; set_visible cascades to all related ids on toggle.
 *
 * @param window        Target window for rendering.
 * @param layer         Layer to register the header element on.
 * @param ctx           Context to wire the left-click toggle handler.
 * @param bridge        Bridge owning the sync loop.
 * @param cursor        Layout cursor. Advanced by @p row_h on return.
 * @param x_min         Left edge in NDC.
 * @param x_max         Right edge in NDC.
 * @param row_h         Header strip height in NDC units.
 * @param initially_open Starting toggle state.
 * @param color_closed  Background color when closed.
 * @param color_open    Background color when open.
 * @param label         Optional GPU image overlaid on the header as a label.
 *                      When provided, the header uses the multi-texture shader;
 *                      when null, a plain color-only shader is used.
 * @return Fully constructed Collapsible. cursor_out is below the header strip.
 */
[[nodiscard]] inline Collapsible make_collapsible(
    const std::shared_ptr<Core::Window>& window,
    Layer& layer,
    Context& ctx,
    Bridge& bridge,
    LayoutCursor& cursor,
    float x_min,
    float x_max,
    float row_h,
    bool initially_open = true,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F),
    std::shared_ptr<Core::VKImage> label = nullptr)
{
    const size_t vertex_count = label ? 12 : 6;
    const size_t cap = vertex_count * Kakshya::VertexLayout::for_meshes().stride_bytes;

    auto buf = label
        ? create_buffer(window, cap,
              Graphics::PrimitiveTopology::TRIANGLE_LIST,
              std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> {
                  { "text", label } })
        : create_buffer(window, cap,
              Graphics::PrimitiveTopology::TRIANGLE_LIST);

    auto cursor_state = cursor.state();
    cursor.advance(row_h);

    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(initially_open);

    Mapped<bool> mapped;
    mapped.state = open_state;
    mapped.geometry_fn = collapsible_header_geom(
        cursor_state, x_min, x_max, row_h, color_closed, color_open, label);
    mapped.element.buffer = buf;
    mapped.element.bounds_hint = Kinesis::AABB2D {
        .min = { x_min, cursor.y() },
        .max = { x_max, cursor.y() + row_h },
    };

    const uint32_t hid = layer.add(mapped.element);
    mapped.element.id = hid;
    open_state->id = hid;
    bridge.register_element(std::move(mapped));

    ctx.on_press(hid, IO::MouseButtons::Left,
        [open = open_state, &layer, hid](uint32_t, glm::vec2) {
            const bool next = !open->value;
            open->write(next);
            for (auto rel_id : layer.related_ids(hid))
                layer.set_visible(rel_id, next);
        });

    return Collapsible {
        .header_id = hid,
        .open = open_state,
        .buf = buf,
        .cursor_out = LayoutCursor(cursor.y()),
    };
}

} // namespace MayaFlux::Portal::Forma
