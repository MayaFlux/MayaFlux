#ifndef VISION_COMMON_GLSL
#define VISION_COMMON_GLSL

// Standard vision shader binding layout:
//   binding 0 — writeonly image2D  out_image  (output storage image)
//   binding 1 — readonly  image2D  in_image   (input storage image)
// Additional bindings declared per-shader after these two.

/**
 * @brief Sample in_image at integer coord, clamping to image bounds.
 *
 * @param img   The source image2D (readonly).
 * @param coord Integer texel coordinate, unclamped.
 * @return      rgba vec4 at the clamped coordinate.
 */
vec4 sample_clamped(readonly image2D img, ivec2 coord)
{
    const ivec2 sz = imageSize(img) - ivec2(1);
    return imageLoad(img, clamp(coord, ivec2(0), sz));
}

/**
 * @brief Write to out_image only when coord is within bounds.
 *
 * @param img   The destination image2D (writeonly).
 * @param coord Integer texel coordinate.
 * @param val   Value to write.
 */
void write_safe(writeonly image2D img, ivec2 coord, vec4 val)
{
    const ivec2 sz = imageSize(img);
    if (coord.x < sz.x && coord.y < sz.y)
        imageStore(img, coord, val);
}

/**
 * @brief Current thread's texel coordinate from GlobalInvocationID.
 */
ivec2 thread_coord()
{
    return ivec2(gl_GlobalInvocationID.xy);
}

#endif // VISION_COMMON_GLSL
