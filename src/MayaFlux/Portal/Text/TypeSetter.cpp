#include "TypeSetter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

#include <utf8proc.h>

namespace MayaFlux::Portal::Text {

bool is_control(uint32_t codepoint) noexcept
{
    switch (utf8proc_category(static_cast<utf8proc_int32_t>(codepoint))) {
    case UTF8PROC_CATEGORY_CC:
    case UTF8PROC_CATEGORY_CF:
    case UTF8PROC_CATEGORY_CS:
    case UTF8PROC_CATEGORY_CO:
        return true;
    default:
        return false;
    }
}

LayoutResult lay_out(
    std::string_view text,
    GlyphAtlas& atlas,
    float pen_x,
    float pen_y,
    uint32_t wrap_w,
    uint32_t tab_width)
{
    if (text.empty()) {
        return { .quads = {}, .final_pen_x = pen_x, .final_pen_y = pen_y };
    }

    LayoutResult out;
    out.quads.reserve(text.size());

    const float origin_x = 0.F;

    {
        const auto* prewarm_bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
        auto prewarm_remaining = static_cast<utf8proc_ssize_t>(text.size());
        utf8proc_ssize_t prewarm_offset = 0;

        while (prewarm_offset < prewarm_remaining) {
            utf8proc_int32_t prewarm_codepoint = 0;
            const utf8proc_ssize_t n = utf8proc_iterate(
                prewarm_bytes + prewarm_offset, prewarm_remaining - prewarm_offset, &prewarm_codepoint);

            if (n <= 0) {
                prewarm_offset += 1;
                continue;
            }
            prewarm_offset += n;

            if (prewarm_codepoint >= 0 && !is_control(static_cast<uint32_t>(prewarm_codepoint))) {
                atlas.get_or_rasterize(static_cast<FT_ULong>(prewarm_codepoint));
            }
        }

        if (tab_width > 0) {
            atlas.get_or_rasterize(static_cast<FT_ULong>(' '));
        }
    }

    FT_UInt prev_glyph_index = 0;
    const FontFace* prev_face = nullptr;

    constexpr float k_wrap_margin = 1.5F;

    const auto apply_wrap = [&](float upcoming_width) -> bool {
        if (wrap_w > 0 && pen_x + upcoming_width * k_wrap_margin > static_cast<float>(wrap_w)) {
            pen_x = origin_x;
            pen_y += static_cast<float>(atlas.line_height());
            prev_glyph_index = 0;
            prev_face = nullptr;
            return true;
        }
        return false;
    };

    const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    auto remaining = static_cast<utf8proc_ssize_t>(text.size());
    utf8proc_ssize_t offset = 0;

    while (offset < remaining) {
        const auto codepoint_start = static_cast<size_t>(offset);
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

        if (is_control(static_cast<uint32_t>(codepoint))) {
            if (codepoint == '\t' && tab_width > 0) {
                const GlyphMetrics* space = atlas.get_or_rasterize(static_cast<FT_ULong>(' '));
                const float stop_width = space && space->advance_x > 0
                    ? static_cast<float>(space->advance_x) * static_cast<float>(tab_width)
                    : 0.F;

                if (stop_width > 0.F) {
                    pen_x = (std::floor((pen_x - origin_x) / stop_width) + 1.F) * stop_width + origin_x;
                    apply_wrap(0.F);
                }
            } else if (codepoint == '\n') {
                pen_x = origin_x;
                pen_y += static_cast<float>(atlas.line_height());
            }

            prev_glyph_index = 0;
            prev_face = nullptr;
            continue;
        }

        const GlyphMetrics* m = atlas.get_or_rasterize(static_cast<FT_ULong>(codepoint));
        if (!m) {
            prev_glyph_index = 0;
            prev_face = nullptr;
            continue;
        }

        apply_wrap(static_cast<float>(m->advance_x));

        if (prev_face == m->face && prev_glyph_index != 0 && m->glyph_index != 0
            && m->face && FT_HAS_KERNING(m->face->get_face())) {
            FT_Vector delta {};
            FT_Get_Kerning(m->face->get_face(), prev_glyph_index, m->glyph_index, FT_KERNING_DEFAULT, &delta);
            pen_x += static_cast<float>(delta.x >> 6);
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
            q.codepoint = static_cast<uint32_t>(codepoint);
            q.uv_y1 = m->uv_y1;
            q.byte_offset = codepoint_start;
            out.quads.push_back(q);
        }

        pen_x += static_cast<float>(m->advance_x);
        prev_glyph_index = m->glyph_index;
        prev_face = m->face;
    }

    out.final_pen_x = pen_x;
    out.final_pen_y = pen_y;
    return out;
}

std::string encode_utf8(uint32_t codepoint)
{
    if (!utf8proc_codepoint_valid(static_cast<utf8proc_int32_t>(codepoint))) {
        return {};
    }

    std::array<utf8proc_uint8_t, 4> buf {};
    const utf8proc_ssize_t n = utf8proc_encode_char(
        static_cast<utf8proc_int32_t>(codepoint), buf.data());

    if (n <= 0) {
        return {};
    }

    return { reinterpret_cast<const char*>(buf.data()), static_cast<size_t>(n) };
}

size_t next_codepoint_offset(std::string_view text, size_t byte_offset)
{
    if (byte_offset >= text.size()) {
        return text.size();
    }

    const auto* bytes = reinterpret_cast<const utf8proc_uint8_t*>(text.data());
    const auto remaining = static_cast<utf8proc_ssize_t>(text.size() - byte_offset);

    utf8proc_int32_t codepoint = 0;
    const utf8proc_ssize_t n = utf8proc_iterate(bytes + byte_offset, remaining, &codepoint);

    return byte_offset + static_cast<size_t>(n > 0 ? n : 1);
}

size_t previous_codepoint_offset(std::string_view text, size_t byte_offset)
{
    if (byte_offset == 0) {
        return 0;
    }

    size_t pos = byte_offset - 1;
    constexpr size_t max_continuation_bytes = 3;
    size_t steps = 0;

    while (pos > 0 && steps < max_continuation_bytes
        && (static_cast<uint8_t>(text[pos]) & 0xC0) == 0x80) {
        --pos;
        ++steps;
    }

    return pos;
}

CopiedText copy(std::string_view text, const LayoutResult& layout, size_t a, size_t b)
{
    CopiedText out;
    out.start = std::min(std::min(a, b), text.size());
    out.end = std::min(std::max(a, b), text.size());

    if (out.start >= out.end) {
        return out;
    }

    out.text = std::string(text.substr(out.start, out.end - out.start));

    for (const auto& q : layout.quads) {
        if (q.byte_offset >= out.start && q.byte_offset < out.end) {
            out.quads.push_back(q);
        }
    }

    return out;
}

} // namespace MayaFlux::Portal::Text
