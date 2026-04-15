#include "RenderText.hpp"

#include <cstddef>

#include <cstddef>

#include "TextLayout.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Kakshya/Source/TextureContainer.hpp"
#include "MayaFlux/Portal/Graphics/GraphicsUtils.hpp"

namespace MayaFlux::Portal::Text {

std::shared_ptr<Buffers::TextureBuffer> render_text(
    std::string_view text,
    GlyphAtlas& atlas,
    glm::vec4 color)
{
    const std::vector<GlyphQuad> quads = lay_out(text, atlas);

    if (quads.empty()) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "render_text: no quads produced for input '{}'",
            std::string(text));
        return nullptr;
    }

    float min_x = quads[0].x0;
    float min_y = quads[0].y0;
    float max_x = quads[0].x1;
    float max_y = quads[0].y1;

    for (const auto& q : quads) {
        min_x = std::min(min_x, q.x0);
        min_y = std::min(min_y, q.y0);
        max_x = std::max(max_x, q.x1);
        max_y = std::max(max_y, q.y1);
    }

    const auto w = static_cast<uint32_t>(std::ceil(max_x - min_x));
    const auto h = static_cast<uint32_t>(std::ceil(max_y - min_y));

    if (w == 0 || h == 0) {
        MF_WARN(Journal::Component::Portal, Journal::Context::API,
            "render_text: zero-size bounding box for '{}'", std::string(text));
        return nullptr;
    }

    const uint8_t cr = static_cast<uint8_t>(std::clamp(color.r, 0.F, 1.F) * 255.F);
    const uint8_t cg = static_cast<uint8_t>(std::clamp(color.g, 0.F, 1.F) * 255.F);
    const uint8_t cb = static_cast<uint8_t>(std::clamp(color.b, 0.F, 1.F) * 255.F);

    Kakshya::TextureContainer canvas(w, h, Portal::Graphics::ImageFormat::RGBA8);
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

        const auto src_x = static_cast<uint32_t>(q.uv_x0 * static_cast<float>(atlas_w));
        const auto src_y = static_cast<uint32_t>(q.uv_y0 * static_cast<float>(atlas_h));

        for (uint32_t row = 0; row < gh; ++row) {
            if (gy + row >= h) {
                break;
            }

            const uint8_t* src_row = src_pixels.data() + static_cast<size_t>((src_y + row) * atlas_w) + src_x;
            uint8_t* dst_row = dst_pixels.data() + static_cast<size_t>(((gy + row) * w + gx) * 4);

            const uint32_t copy_w = std::min(gw, w - gx);
            for (uint32_t col = 0; col < copy_w; ++col) {
                const uint8_t coverage = src_row[col];
                const uint8_t alpha = static_cast<uint8_t>(
                    (static_cast<uint32_t>(coverage) * static_cast<uint32_t>(static_cast<uint8_t>(std::clamp(color.a, 0.F, 1.F) * 255.F))) / 255U);
                dst_row[col * 4 + 0] = cr;
                dst_row[col * 4 + 1] = cg;
                dst_row[col * 4 + 2] = cb;
                dst_row[col * 4 + 3] = alpha;
            }
        }
    }

    auto buffer = std::make_shared<Buffers::TextureBuffer>(
        w, h,
        Portal::Graphics::ImageFormat::RGBA8,
        dst_pixels.data());

    MF_DEBUG(Journal::Component::Portal, Journal::Context::API,
        "render_text: '{}' -> {}x{} TextureBuffer", std::string(text), w, h);

    return buffer;
}

} // namespace MayaFlux::Portal::Text
