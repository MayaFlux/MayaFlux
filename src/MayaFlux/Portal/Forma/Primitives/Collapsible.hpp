#pragma once

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
 * A named primitive: a header quad whose open/closed MappedState drives
 * both geometry color and Layer visibility of all related elements.
 */
struct Collapsible {
    uint32_t header_id {};
    std::shared_ptr<MappedState<bool>> open;
    std::shared_ptr<Buffers::FormaBuffer> buf;
    LayoutCursor cursor_out;
};

/**
 * @brief Geometry function for a collapsible header strip.
 *
 * Writes 12 vertices (TRIANGLE_LIST) at for_meshes() stride.
 * weight=0 for background quad (vertex color), weight=1 for text quad.
 * Text quad UVs span [0,1]. Color quad color changes with open state.
 */
[[nodiscard]] inline GeometryFn<bool> collapsible_header_geom(
    std::shared_ptr<MappedState<float>> cursor_in,
    float x_min,
    float x_max,
    float row_h,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F))
{
    return [cursor_in, x_min, x_max, row_h, color_closed, color_open](
               bool open, std::vector<uint8_t>& out, Element& el) {
        const float top = cursor_in->value;
        const float bot = top - row_h;
        const glm::vec3 col = open ? color_open : color_closed;

        const auto layout = Kakshya::VertexLayout::for_meshes();
        const size_t stride = layout.stride_bytes;

        const glm::vec3 bl { x_min, bot, 0.F };
        const glm::vec3 br { x_max, bot, 0.F };
        const glm::vec3 tl { x_min, top, 0.F };
        const glm::vec3 tr { x_max, top, 0.F };

        constexpr glm::vec3 white { 1.F, 1.F, 1.F };
        constexpr float w0 = 0.F;
        constexpr float w1 = 1.F;
        const glm::vec2 uv_bl { 0.F, 1.F };
        const glm::vec2 uv_br { 1.F, 1.F };
        const glm::vec2 uv_tl { 0.F, 0.F };
        const glm::vec2 uv_tr { 1.F, 0.F };

        out.assign(12 * stride, 0);

        auto write = [&](size_t idx, const glm::vec3& pos, const glm::vec3& c,
                         float w, const glm::vec2& uv) {
            auto* v = out.data() + idx * stride;
            std::memcpy(v, &pos, 12);
            std::memcpy(v + 12, &c, 12);
            std::memcpy(v + 24, &w, 4);
            std::memcpy(v + 28, &uv, 8);
        };

        // Background (weight=0)
        write(0, bl, col, w0, {});
        write(1, br, col, w0, {});
        write(2, tl, col, w0, {});
        write(3, br, col, w0, {});
        write(4, tr, col, w0, {});
        write(5, tl, col, w0, {});

        // Text quad (weight=1)
        write(6, bl, white, w1, uv_bl);
        write(7, br, white, w1, uv_br);
        write(8, tl, white, w1, uv_tl);
        write(9, br, white, w1, uv_br);
        write(10, tr, white, w1, uv_tr);
        write(11, tl, white, w1, uv_tl);

        el.bounds_hint = Kinesis::AABB2D {
            .min = { x_min, bot },
            .max = { x_max, top },
        };
    };
}

/**
 * @brief Construct a Collapsible from a caller-owned, caller-registered FormaBuffer.
 *
 * Does not construct, register, or call setup_rendering on the buffer.
 * The caller owns all of those steps. setup_rendering should be called
 * after make_collapsible returns, once any textures are known.
 *
 * Advances @p cursor by @p row_h.
 *
 * @param buf            Registered FormaBuffer. setup_rendering not yet called.
 * @param layer          Layer to register the header element on.
 * @param bridge         Bridge owning the sync loop.
 * @param cursor         Layout cursor. Advanced by row_h internally.
 * @param x_min          Left edge in NDC.
 * @param x_max          Right edge in NDC.
 * @param row_h          Header strip height in NDC units.
 * @param initially_open Starting toggle state.
 * @param color_closed   Header color when closed.
 * @param color_open     Header color when open.
 */
[[nodiscard]] inline Collapsible make_collapsible(
    std::shared_ptr<Buffers::FormaBuffer> buf,
    Layer& layer,
    Bridge& bridge,
    LayoutCursor& cursor,
    float x_min,
    float x_max,
    float row_h,
    bool initially_open = true,
    glm::vec3 color_closed = glm::vec3(0.25F),
    glm::vec3 color_open = glm::vec3(0.35F))
{
    auto cursor_state = cursor.state();
    cursor.advance(row_h);

    auto open_state = std::make_shared<MappedState<bool>>();
    open_state->write(initially_open);

    Mapped<bool> mapped;
    mapped.state = open_state;
    mapped.geometry_fn = collapsible_header_geom(
        cursor_state, x_min, x_max, row_h, color_closed, color_open);
    mapped.element.buffer = buf;
    mapped.element.bounds_hint = Kinesis::AABB2D {
        .min = { x_min, cursor.y() },
        .max = { x_max, cursor.y() + row_h },
    };

    const uint32_t hid = layer.add(mapped.element);
    open_state->id = hid;
    bridge.register_element(std::move(mapped));

    return Collapsible {
        .header_id = hid,
        .open = open_state,
        .buf = std::move(buf),
        .cursor_out = LayoutCursor(cursor.y()),
    };
}

} // namespace MayaFlux::Portal::Forma
