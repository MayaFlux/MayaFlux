#include "InkPress.hpp"

#include "TypeFaceFoundry.hpp"
#include "TypeSetter.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Portal::Text {

namespace {

    constexpr uint32_t k_grow_height_multiplier = 8;

    /**
     * @brief Pixel-raster result of a single composite pass.
     *
     * w is always the render bounds width (== the wrap boundary).
     * h is the content height: the row count actually written.
     * cursor_x is the pen x position after the last glyph, for cursor seeding.
     * pixels is row-major RGBA8, stride == w * 4.
     */
    struct CompositeResult {
        uint32_t h {};
        uint32_t cursor_x {};
        uint32_t cursor_y {};
        std::vector<uint8_t> pixels;
    };

    /**
     * @brief Resolve atlas pointer: use provided atlas or fall back to foundry default.
     * @return Non-null pointer on success, nullptr if no atlas is available.
     */
    GlyphAtlas* resolve_atlas(GlyphAtlas* hint)
    {
        if (hint) {
            return hint;
        }
        GlyphAtlas* def = TypeFaceFoundry::instance().get_default_glyph_atlas();
        if (!def) {
            MF_ERROR(Journal::Component::Portal, Journal::Context::API,
                "InkPress: no atlas available -- call set_default_font first");
        }
        return def;
    }

    /**
     * @brief Lay out and rasterize text into a fresh pixel buffer.
     *
     * Always lays out from pen origin (0, 0). The pixel buffer width is
     * exactly buf_w (== render bounds width). Content that wraps beyond
     * buf_h is clipped by composite_into().
     *
     * @param text   UTF-8 string.
     * @param atlas  Source atlas.
     * @param color  Glyph color.
     * @param buf_w  Buffer width and wrap boundary in pixels.
     * @param buf_h  Buffer height (allocated budget) in pixels.
     * @return       CompositeResult, or nullopt when no glyphs are produced.
     */
    std::optional<CompositeResult> composite(
        std::string_view text,
        GlyphAtlas& atlas,
        glm::vec4 color,
        uint32_t buf_w,
        uint32_t buf_h,
        float pen_y_start = 0.F)
    {
        const LayoutResult layout = lay_out(text, atlas, 0.F, pen_y_start, buf_w);
        if (layout.quads.empty()) {
            return std::nullopt;
        }

        const auto content_h = static_cast<uint32_t>(std::ceil(layout.final_pen_y))
            + atlas.line_height();
        const uint32_t dst_h = std::min(content_h, buf_h);

        if (buf_w == 0 || dst_h == 0) {
            return std::nullopt;
        }

        CompositeResult result;
        result.h = content_h;
        result.cursor_x = static_cast<uint32_t>(std::ceil(layout.final_pen_x));
        result.cursor_y = static_cast<uint32_t>(std::ceil(layout.final_pen_y));
        result.pixels.resize(static_cast<size_t>(buf_w) * dst_h * 4, 0);

        rasterize_quads(layout.quads, atlas, color, result.pixels.data(), buf_w, dst_h);

        return result;
    }

    /**
     * @brief Allocate a TextBuffer from a CompositeResult.
     *
     * The GPU texture is always buf_w x budget_h. Content rows from result
     * are copied in; remaining rows are zero (transparent).
     *
     * @param result        Output of composite().
     * @param buf_w         Texture width == render bounds width.
     * @param budget_h      Allocated texture height (>= result.h).
     * @param render_bounds Hard render bounds stored on the buffer.
     * @param text          Accumulated text string seeded on the buffer.
     */
    std::shared_ptr<Buffers::TextBuffer> make_buffer(
        const CompositeResult& result,
        uint32_t buf_w,
        uint32_t budget_h,
        glm::uvec2 render_bounds,
        std::string_view text)
    {
        budget_h = std::max(budget_h, result.h);

        const size_t budget_bytes = static_cast<size_t>(buf_w) * budget_h * 4;
        std::vector<uint8_t> pixels(budget_bytes, 0);

        const uint32_t copy_h = std::min(result.h,
            static_cast<uint32_t>(result.pixels.size() / (buf_w * 4)));
        for (uint32_t row = 0; row < copy_h; ++row) {
            std::memcpy(
                pixels.data() + static_cast<size_t>(row) * buf_w * 4,
                result.pixels.data() + static_cast<size_t>(row) * buf_w * 4,
                static_cast<size_t>(buf_w) * 4);
        }

        auto buf = std::make_shared<Buffers::TextBuffer>(
            buf_w, budget_h,
            Portal::Graphics::ImageFormat::RGBA8,
            pixels.data());

        buf->set_budget(buf_w, budget_h);
        buf->set_render_bounds(render_bounds.x, render_bounds.y);
        buf->set_accumulated_text(text);
        buf->get_cursor_x() = result.cursor_x;
        buf->get_cursor_y() = result.cursor_y;

        MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
            "InkPress: {}x{} content in {}x{} texture, render bounds {}x{}",
            buf_w, result.h, buf_w, budget_h, render_bounds.x, render_bounds.y);

        return buf;
    }

} // namespace

void rasterize_quads(
    std::span<const GlyphQuad> quads,
    GlyphAtlas& atlas,
    glm::vec4 color,
    uint8_t* dst,
    uint32_t buf_w,
    uint32_t buf_h)
{
    const uint8_t cr = static_cast<uint8_t>(std::clamp(color.r, 0.F, 1.F) * 255.F);
    const uint8_t cg = static_cast<uint8_t>(std::clamp(color.g, 0.F, 1.F) * 255.F);
    const uint8_t cb = static_cast<uint8_t>(std::clamp(color.b, 0.F, 1.F) * 255.F);
    const uint8_t ca = static_cast<uint8_t>(std::clamp(color.a, 0.F, 1.F) * 255.F);

    const Kakshya::TextureContainer& atlas_tex = atlas.texture();
    const std::span<const uint8_t> atlas_pixels = atlas_tex.pixel_bytes(0);
    const uint32_t atlas_size = atlas.atlas_size();

    for (const auto& q : quads) {
        const auto gx = static_cast<int32_t>(std::floor(q.x0));
        const auto gy = static_cast<int32_t>(std::floor(q.y0));
        const auto gw = static_cast<uint32_t>(std::ceil(q.x1 - q.x0));
        const auto gh = static_cast<uint32_t>(std::ceil(q.y1 - q.y0));

        const auto src_x = static_cast<uint32_t>(q.uv_x0 * static_cast<float>(atlas_size));
        const auto src_y = static_cast<uint32_t>(q.uv_y0 * static_cast<float>(atlas_size));

        for (uint32_t row = 0; row < gh; ++row) {
            const int32_t dst_row = gy + static_cast<int32_t>(row);
            if (dst_row < 0 || static_cast<uint32_t>(dst_row) >= buf_h) {
                continue;
            }

            const uint8_t* src_row = atlas_pixels.data()
                + static_cast<size_t>(src_y + row) * atlas_size + src_x;
            uint8_t* dst_row_ptr = dst + static_cast<size_t>(dst_row) * buf_w * 4;

            for (uint32_t col = 0; col < gw; ++col) {
                const int32_t dst_col = gx + static_cast<int32_t>(col);
                if (dst_col < 0 || static_cast<uint32_t>(dst_col) >= buf_w) {
                    continue;
                }

                const uint8_t coverage = src_row[col];
                const auto alpha = static_cast<uint8_t>(
                    (static_cast<uint32_t>(coverage) * static_cast<uint32_t>(ca)) / 255U);

                uint8_t* px = dst_row_ptr + static_cast<size_t>(dst_col) * 4;
                px[0] = cr;
                px[1] = cg;
                px[2] = cb;
                px[3] = alpha;
            }
        }
    }
}

void ink_quads(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::span<const GlyphQuad> quads,
    glm::vec4 color)
{
    if (!target) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "ink_quads: target buffer is null");
        return;
    }

    GlyphAtlas* atlas = resolve_atlas(nullptr);
    if (!atlas) {
        return;
    }

    const uint32_t buf_w = target->get_budget_width();
    const uint32_t buf_h = target->get_budget_height();
    const size_t buf_bytes = static_cast<size_t>(buf_w) * buf_h * 4;

    thread_local std::vector<uint8_t> pixels;
    pixels.assign(buf_bytes, 0);

    rasterize_quads(quads, *atlas, color, pixels.data(), buf_w, buf_h);
    target->set_pixel_data(pixels.data(), buf_bytes);
}

// =========================================================================
// press
// =========================================================================

std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    const PressParams& params)
{
    GlyphAtlas* atlas = resolve_atlas(params.atlas);
    if (!atlas) {
        return nullptr;
    }

    const uint32_t buf_w = params.render_bounds.x;

    const auto result = composite(text, *atlas, params.color, buf_w, params.render_bounds.y,
        static_cast<float>(atlas->line_height()));
    if (!result) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "press: no glyphs produced for '{}'", std::string(text));
        return nullptr;
    }

    const uint32_t budget_h = params.budget_h > 0
        ? std::max(params.budget_h, result->h)
        : std::min(result->h * k_grow_height_multiplier, params.render_bounds.y);

    return make_buffer(*result, buf_w, budget_h, params.render_bounds, text);
}

// =========================================================================
// repress
// =========================================================================

bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color,
    RedrawPolicy policy)
{
    if (!target) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "repress: target buffer is null");
        return false;
    }

    GlyphAtlas* atlas = resolve_atlas(nullptr);
    if (!atlas) {
        return false;
    }

    target->clear_accumulated_text();
    target->reset_cursor();

    const uint32_t buf_w = target->get_budget_width();
    const uint32_t buf_h = target->get_budget_height();
    const uint32_t bound_h = target->get_render_bounds_h();

    const auto result = composite(text, *atlas, color, buf_w, bound_h,
        static_cast<float>(atlas->line_height()));
    if (!result) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "repress: no glyphs produced for '{}'", std::string(text));
        return false;
    }

    if (result->h > buf_h && policy == RedrawPolicy::Fit) {
        const uint32_t new_h = std::min(result->h, bound_h);
        target->resize_texture(buf_w, new_h);
        target->set_budget(buf_w, new_h);
        target->set_pixel_data(result->pixels.data(), result->pixels.size());
        target->get_cursor_x() = result->cursor_x;
        target->get_cursor_y() = result->cursor_y;
        target->set_accumulated_text(text);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
            "repress(Fit): resized to {}x{}", buf_w, new_h);
        return true;
    }

    const size_t buf_bytes = static_cast<size_t>(buf_w) * buf_h * 4;
    std::vector<uint8_t> cleared(buf_bytes, 0);

    const uint32_t copy_h = std::min(result->h, buf_h);
    for (uint32_t row = 0; row < copy_h; ++row) {
        std::memcpy(
            cleared.data() + static_cast<size_t>(row) * buf_w * 4,
            result->pixels.data() + static_cast<size_t>(row) * buf_w * 4,
            static_cast<size_t>(buf_w) * 4);
    }

    target->set_pixel_data(cleared.data(), buf_bytes);
    target->get_cursor_x() = result->cursor_x;
    target->get_cursor_y() = result->cursor_y;
    // target->get_cursor_y() = result->h;
    target->set_accumulated_text(text);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
        "repress(Clip): '{}' -> {}x{} into {}x{} budget",
        std::string(text), buf_w, result->h, buf_w, buf_h);

    return true;
}

// =========================================================================
// impress
// =========================================================================

ImpressResult impress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    std::string_view text,
    glm::vec4 color)
{
    if (!target) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "impress: target buffer is null");
        return ImpressResult::Overflow;
    }

    GlyphAtlas* atlas = resolve_atlas(nullptr);
    if (!atlas) {
        return ImpressResult::Overflow;
    }

    const uint32_t buf_w = target->get_budget_width();
    const uint32_t buf_h = target->get_budget_height();
    const uint32_t bound_h = target->get_render_bounds_h();
    const auto pen_x = static_cast<float>(target->get_cursor_x());
    const auto pen_y = static_cast<float>(target->get_cursor_y());

    target->append_accumulated_text(text);

    const LayoutResult layout = lay_out(text, *atlas, pen_x, pen_y, buf_w);

    if (layout.quads.empty()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "impress: no glyphs produced for '{}'", std::string(text));
        return ImpressResult::Ok;
    }

    const auto pen_y_ceil = static_cast<uint32_t>(std::ceil(layout.final_pen_y));
    const uint32_t new_cursor_y = pen_y_ceil;
    const uint32_t content_h = pen_y_ceil + atlas->line_height();

    if (content_h > bound_h) {
        return ImpressResult::Overflow;
    }

    if (content_h > buf_h) {
        const uint32_t new_h = std::min(content_h * k_grow_height_multiplier, bound_h);
        const std::string accumulated = target->get_accumulated_text();

        const auto full = composite(accumulated, *atlas, color, buf_w, new_h);
        if (!full) {
            return ImpressResult::Overflow;
        }

        target->resize_texture(buf_w, new_h);
        target->set_budget(buf_w, new_h);

        const size_t buf_bytes = static_cast<size_t>(buf_w) * new_h * 4;
        std::vector<uint8_t> pixels(buf_bytes, 0);
        const uint32_t copy_h = std::min(full->h, new_h);
        for (uint32_t row = 0; row < copy_h; ++row) {
            std::memcpy(
                pixels.data() + static_cast<size_t>(row) * buf_w * 4,
                full->pixels.data() + static_cast<size_t>(row) * buf_w * 4,
                static_cast<size_t>(buf_w) * 4);
        }

        target->set_pixel_data(pixels.data(), buf_bytes);
        target->get_cursor_x() = full->cursor_x;
        target->get_cursor_y() = full->h;
        target->set_accumulated_text(accumulated);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
            "impress: vertical grow -> {}x{}", buf_w, new_h);

        return ImpressResult::Overflow;
    }

    auto& pixel_data = target->get_pixel_data_mutable();
    rasterize_quads(layout.quads, *atlas, color, pixel_data.data(), buf_w, buf_h);
    target->mark_pixels_dirty();
    target->get_cursor_x() = static_cast<uint32_t>(std::ceil(layout.final_pen_x));
    target->get_cursor_y() = new_cursor_y;

    MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
        "impress: '{}' at ({},{}) -> cursor ({},{})",
        std::string(text),
        static_cast<uint32_t>(pen_x), static_cast<uint32_t>(pen_y),
        target->get_cursor_x(), target->get_cursor_y());

    return ImpressResult::Ok;
}

} // namespace MayaFlux::Portal::Text
