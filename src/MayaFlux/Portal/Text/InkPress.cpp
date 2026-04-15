#include "InkPress.hpp"

#include "TypeFaceFoundry.hpp"
#include "TypeSetter.hpp"

#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Portal::Text {

namespace {

    struct CompositeResult {
        uint32_t w {};
        uint32_t h {};
        std::vector<uint8_t> pixels;
    };

    std::optional<CompositeResult> composite(
        std::string_view text,
        GlyphAtlas& atlas,
        glm::vec4 color,
        uint32_t max_w = 0,
        uint32_t max_h = 0)
    {
        const std::vector<GlyphQuad> quads = lay_out(text, atlas);
        if (quads.empty()) {
            return std::nullopt;
        }

        float min_x = quads[0].x0, min_y = quads[0].y0;
        float max_x = quads[0].x1, max_y = quads[0].y1;
        for (const auto& q : quads) {
            min_x = std::min(min_x, q.x0);
            min_y = std::min(min_y, q.y0);
            max_x = std::max(max_x, q.x1);
            max_y = std::max(max_y, q.y1);
        }

        const auto w = static_cast<uint32_t>(std::ceil(max_x - min_x));
        const auto h = static_cast<uint32_t>(std::ceil(max_y - min_y));

        if (w == 0 || h == 0) {
            return std::nullopt;
        }

        const uint32_t dst_w = max_w > 0 ? std::min(w, max_w) : w;
        const uint32_t dst_h = max_h > 0 ? std::min(h, max_h) : h;

        const uint8_t cr = static_cast<uint8_t>(std::clamp(color.r, 0.F, 1.F) * 255.F);
        const uint8_t cg = static_cast<uint8_t>(std::clamp(color.g, 0.F, 1.F) * 255.F);
        const uint8_t cb = static_cast<uint8_t>(std::clamp(color.b, 0.F, 1.F) * 255.F);

        Kakshya::TextureContainer canvas(dst_w, dst_h, Portal::Graphics::ImageFormat::RGBA8);
        const std::span<uint8_t> dst_pixels = canvas.pixel_bytes(0);

        const Kakshya::TextureContainer& atlas_tex = atlas.texture();
        const std::span<const uint8_t> src_pixels = atlas_tex.pixel_bytes(0);
        const uint32_t atlas_w = atlas.atlas_size();
        const uint32_t atlas_h = atlas.atlas_size();

        for (const auto& q : quads) {
            const auto gx = static_cast<uint32_t>(q.x0 - min_x);
            const auto gy = static_cast<uint32_t>(q.y0 - min_y);
            const auto gw = static_cast<uint32_t>(q.x1 - q.x0);
            const auto gh = static_cast<uint32_t>(q.y1 - q.y0);

            if (gx >= dst_w || gy >= dst_h) {
                continue;
            }

            const auto src_x = static_cast<uint32_t>(q.uv_x0 * static_cast<float>(atlas_w));
            const auto src_y = static_cast<uint32_t>(q.uv_y0 * static_cast<float>(atlas_h));

            for (uint32_t row = 0; row < gh; ++row) {
                if (gy + row >= dst_h) {
                    break;
                }
                const uint8_t* src_row = src_pixels.data()
                    + static_cast<size_t>((src_y + row) * atlas_w) + src_x;
                uint8_t* dst_row = dst_pixels.data()
                    + static_cast<size_t>(((gy + row) * dst_w + gx) * 4);

                const uint32_t copy_w = std::min(gw, dst_w - gx);
                for (uint32_t col = 0; col < copy_w; ++col) {
                    const uint8_t coverage = src_row[col];
                    const uint8_t alpha = static_cast<uint8_t>(
                        (static_cast<uint32_t>(coverage)
                            * static_cast<uint32_t>(
                                static_cast<uint8_t>(std::clamp(color.a, 0.F, 1.F) * 255.F)))
                        / 255U);
                    dst_row[col * 4 + 0] = cr;
                    dst_row[col * 4 + 1] = cg;
                    dst_row[col * 4 + 2] = cb;
                    dst_row[col * 4 + 3] = alpha;
                }
            }
        }

        CompositeResult result;
        result.w = dst_w;
        result.h = dst_h;
        result.pixels.assign(dst_pixels.begin(), dst_pixels.end());
        return result;
    }

} // namespace

// =========================================================================
// press
// =========================================================================

std::shared_ptr<Buffers::TextBuffer> press(std::string_view text, glm::vec4 color)
{
    GlyphAtlas* atlas = TypeFaceFoundry::instance().get_default_glyph_atlas();
    if (!atlas) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "press: no default atlas -- call set_default_font first");
        return nullptr;
    }
    return press(text, *atlas, color);
}

std::shared_ptr<Buffers::TextBuffer> press(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color)
{
    const auto result = composite(text, atlas, color);
    if (!result) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "press: no glyphs produced for '{}'", std::string(text));
        return nullptr;
    }

    auto buffer = std::make_shared<Buffers::TextBuffer>(
        result->w, result->h,
        Portal::Graphics::ImageFormat::RGBA8,
        result->pixels.data());

    MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
        "press: '{}' -> {}x{} TextBuffer", std::string(text), result->w, result->h);

    return buffer;
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
    GlyphAtlas* atlas = TypeFaceFoundry::instance().get_default_glyph_atlas();
    if (!atlas) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "repress: no default atlas -- call set_default_font first");
        return false;
    }
    return repress(target, *atlas, text, color, policy);
}

bool repress(
    const std::shared_ptr<Buffers::TextBuffer>& target,
    GlyphAtlas& atlas,
    std::string_view text,
    glm::vec4 color,
    RedrawPolicy policy)
{
    if (!target) {
        MF_ERROR(Journal::Component::Portal, Journal::Context::API,
            "repress: target buffer is null");
        return false;
    }

    const uint32_t buf_w = target->get_width();
    const uint32_t buf_h = target->get_height();

    const auto result = composite(text, atlas, color);
    if (!result) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "repress: no glyphs produced for '{}'", std::string(text));
        return false;
    }

    const bool exceeds = result->w > buf_w || result->h > buf_h;

    if (exceeds && policy == RedrawPolicy::Strict) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "repress: text {}x{} exceeds buffer {}x{} (Strict)",
            result->w, result->h, buf_w, buf_h);
        return false;
    }

    if (exceeds && policy == RedrawPolicy::Grow) {
        // target->resize_texture(result->w, result->h);
        // target->set_pixel_data(result->pixels.data(), result->pixels.size());

        target->resize_texture(result->w * 4, result->h * 4);
        const size_t new_bytes = static_cast<size_t>(result->w) * result->h * 4;
        target->set_pixel_data(result->pixels.data(), new_bytes);

        MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
            "repress: grew buffer to {}x{}", result->w, result->h);
        return true;
    }

    const size_t buf_bytes = static_cast<size_t>(buf_w) * buf_h * 4;
    std::vector<uint8_t> cleared(buf_bytes, 0);

    const uint32_t copy_w = std::min(result->w, buf_w);
    const uint32_t copy_h = std::min(result->h, buf_h);
    for (uint32_t row = 0; row < copy_h; ++row) {
        std::memcpy(
            cleared.data() + static_cast<size_t>(row * buf_w * 4),
            result->pixels.data() + static_cast<size_t>(row * result->w * 4),
            static_cast<size_t>(copy_w) * 4);
    }

    target->set_pixel_data(cleared.data(), buf_bytes);

    MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
        "repress: '{}' -> {}x{} into {}x{} buffer",
        std::string(text), result->w, result->h, buf_w, buf_h);

    return true;
}

} // namespace MayaFlux::Portal::Text
