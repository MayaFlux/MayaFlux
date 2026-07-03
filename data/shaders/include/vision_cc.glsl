#ifndef VISION_CC_GLSL
#define VISION_CC_GLSL

/**
 * @brief Sentinel label value marking background / unclaimed pixels.
 */
const uint CC_BACKGROUND = 0xFFFFFFFFu;

/**
 * @brief Sentinel marking an unclaimed slot in the compaction lookup table.
 */
const uint CC_UNCLAIMED = 0u;

/**
 * @brief Flatten a 2D pixel coordinate to the linear label-buffer index.
 */
uint cc_flat_index(uvec2 coord, uint width)
{
    return coord.y * width + coord.x;
}

#endif // VISION_CC_GLSL
