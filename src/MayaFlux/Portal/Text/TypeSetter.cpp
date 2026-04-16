#include "TypeSetter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <utf8proc.h>

namespace MayaFlux::Portal::Text {

LayoutResult lay_out(
    std::string_view text,
    GlyphAtlas& atlas,
    float pen_x,
    float pen_y,
    uint32_t wrap_w)
{
    if (text.empty()) {
        return { .quads = {}, .final_pen_x = pen_x, .final_pen_y = pen_y };
    }

    LayoutResult out;
    out.quads.reserve(text.size());

    const float origin_x = 0.F;

    const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    utf8proc_ssize_t offset = 0;

    while (offset < remaining) {
        utf8proc_int32_t codepoint = 0;
        const utf8proc_ssize_t n = utf8proc_iterate(bytes + offset, remaining - offset, &codepoint);

        if (n <= 0) {
            MF_WARN(Journal::Component::Portal, Journal::Context::API,
                "TypeSetter: invalid UTF-8 sequence at byte offset {}, skipping byte",
                static_cast<size_t>(offset));
            offset += 1;
            continue;
        }

        offset += n;

        if (codepoint < 0) {
            continue;
        }

        if (codepoint == '\n') {
            pen_x = origin_x;
            pen_y += static_cast<float>(atlas.line_height());
            continue;
        }

        if (codepoint == '\r') {
            continue;
        }

        const GlyphMetrics* m = atlas.get_or_rasterize(static_cast<FT_ULong>(codepoint));
        if (!m) {
            continue;
        }

        if (m->width > 0 && m->height > 0) {
            GlyphQuad q {};
            q.x0 = pen_x + static_cast<float>(m->bearing_x);
            q.y0 = pen_y - static_cast<float>(m->bearing_y);
            q.x1 = q.x0 + static_cast<float>(m->width);
            q.y1 = q.y0 + static_cast<float>(m->height);
            q.uv_x0 = m->uv_x0;
            q.uv_y0 = m->uv_y0;
            q.uv_x1 = m->uv_x1;
            q.uv_y1 = m->uv_y1;
            out.quads.push_back(q);
        }

        pen_x += static_cast<float>(m->advance_x);

        if (wrap_w > 0 && static_cast<uint32_t>(std::ceil(pen_x)) > wrap_w) {
            pen_x = 0.F;
            pen_y += static_cast<float>(atlas.line_height());
        }
    }

    out.final_pen_x = pen_x;
    out.final_pen_y = pen_y;
    return out;
}

} // namespace MayaFlux::Portal::Text
