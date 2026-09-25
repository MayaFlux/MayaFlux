#pragma once

/**
 * @file Pyramid.hpp
 * @brief Placement of an image pyramid inside a single atlas.
 *
 * Pure geometry. No MayaFlux type dependencies and no notion of what the
 * texels hold, so the same layout serves a CPU pyramid, a GPU atlas image, or
 * a test that checks where a level landed.
 */

namespace MayaFlux::Kinesis::Vision {

/**
 * @brief Extent and position of one pyramid level inside an atlas.
 */
struct PyramidLevel {
    uint32_t ox { 0 };
    uint32_t oy { 0 };
    uint32_t w { 0 };
    uint32_t h { 0 };
};

/**
 * @brief Placement of every pyramid level in a single atlas.
 *
 * Level 0 occupies the left block at full size. Deeper levels are stacked in a
 * column to its right, each half the size of the one before (rounded up), so
 * the whole pyramid is one image.
 */
struct PyramidLayout {
    static constexpr uint32_t k_max_levels = 6;

    std::array<PyramidLevel, k_max_levels> level {};
    uint32_t levels { 0 };
    uint32_t atlas_w { 0 };
    uint32_t atlas_h { 0 };

    /**
     * @brief Layouts are equal when they describe the same atlas for the same
     *        base extent; every other field follows from those.
     */
    [[nodiscard]] bool operator==(const PyramidLayout& other) const noexcept
    {
        return levels == other.levels && atlas_w == other.atlas_w && atlas_h == other.atlas_h
            && level[0].w == other.level[0].w && level[0].h == other.level[0].h;
    }
};

/**
 * @brief Place up to @p requested pyramid levels for a w x h base image.
 *
 * Levels stop early when the next one would have a side under @p min_extent,
 * so small images get fewer levels rather than degenerate ones. @p requested
 * is clamped to [1, PyramidLayout::k_max_levels].
 *
 * @param w          Base width in pixels.
 * @param h          Base height in pixels.
 * @param requested  Desired number of levels including the base.
 * @param min_extent Smallest side a level may have.
 * @return           Layout with level 0 at the origin and the rest stacked to its right.
 */
[[nodiscard]] inline PyramidLayout pyramid_layout(
    uint32_t w, uint32_t h, uint32_t requested, uint32_t min_extent = 16)
{
    PyramidLayout layout;
    layout.level[0] = { .ox = 0, .oy = 0, .w = w, .h = h };

    const uint32_t wanted = std::clamp(requested, 1U, PyramidLayout::k_max_levels);
    uint32_t count = 1;
    uint32_t column_h = 0;
    uint32_t column_w = 0;

    while (count < wanted) {
        const auto& above = layout.level[count - 1];
        const uint32_t lw = (above.w + 1U) / 2U;
        const uint32_t lh = (above.h + 1U) / 2U;
        if (std::min(lw, lh) < min_extent)
            break;

        layout.level[count] = { .ox = w, .oy = column_h, .w = lw, .h = lh };
        column_h += lh;
        column_w = std::max(column_w, lw);
        ++count;
    }

    layout.levels = count;
    layout.atlas_w = w + column_w;
    layout.atlas_h = std::max(h, column_h);
    return layout;
}

} // namespace MayaFlux::Kinesis::Vision
