#pragma once

#include "Scissor.hpp"

namespace MayaFlux::Kinesis {

/**
 * @struct ViewportArea
 * @brief The part of a layered surface that clip space is mapped onto.
 *
 * The layer is the array layer or cube face drawn into. The area is a region of
 * that layer in the NDC convention of Scissor, so it stays right when the
 * surface is resized. Unset is the whole layer. A destination drawing into an
 * image target fills the area with its view and is clipped to it, together
 * with its own scissor, which is resolved against the area.
 */
struct ViewportArea {
    uint32_t layer {};
    std::optional<Scissor> area;

    [[nodiscard]] bool operator==(const ViewportArea& other) const noexcept
    {
        return layer == other.layer && area.has_value() == other.area.has_value()
            && (!area || *area == *other.area);
    }

    /** @brief The pixels of the area inside a surface of the given size. All of it when unset. */
    [[nodiscard]] PixelRect pixels(uint32_t width, uint32_t height) const noexcept
    {
        if (!area) {
            return { .x0 = 0U, .y0 = 0U, .x1 = width, .y1 = height };
        }

        const auto fw = static_cast<float>(width);
        const auto fh = static_cast<float>(height);
        const auto& bounds = area->bounds;

        const float x0 = std::clamp((bounds.min.x * 0.5F + 0.5F) * fw, 0.F, fw);
        const float x1 = std::clamp((bounds.max.x * 0.5F + 0.5F) * fw, 0.F, fw);
        const float y0 = std::clamp((1.F - bounds.max.y) * 0.5F * fh, 0.F, fh);
        const float y1 = std::clamp((1.F - bounds.min.y) * 0.5F * fh, 0.F, fh);

        const auto left = static_cast<uint32_t>(x0);
        const auto top = static_cast<uint32_t>(y0);
        return {
            .x0 = left,
            .y0 = top,
            .x1 = left + static_cast<uint32_t>(x1 - x0),
            .y1 = top + static_cast<uint32_t>(y1 - y0)
        };
    }
};

} // namespace MayaFlux::Kinesis
