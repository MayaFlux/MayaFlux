#ifndef VISION_MORPHOLOGY_GLSL
#define VISION_MORPHOLOGY_GLSL

#include "vision_common.glsl"

/**
 * @brief Morphological erosion: minimum value over a (2*radius+1)^2 neighbourhood.
 *
 * @param img    Source image (readonly).
 * @param coord  Centre coordinate.
 * @param radius Half-width of the structuring element.
 * @return       Per-channel minimum across the neighbourhood.
 */
vec4 morph_erode(readonly image2D img, ivec2 coord, int radius)
{
    vec4 result = vec4(1.0);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            result = min(result, sample_clamped(img, coord + ivec2(dx, dy)));
    return result;
}

/**
 * @brief Morphological dilation: maximum value over a (2*radius+1)^2 neighbourhood.
 *
 * @param img    Source image (readonly).
 * @param coord  Centre coordinate.
 * @param radius Half-width of the structuring element.
 * @return       Per-channel maximum across the neighbourhood.
 */
vec4 morph_dilate(readonly image2D img, ivec2 coord, int radius)
{
    vec4 result = vec4(0.0);
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            result = max(result, sample_clamped(img, coord + ivec2(dx, dy)));
    return result;
}

#endif // VISION_MORPHOLOGY_GLSL
