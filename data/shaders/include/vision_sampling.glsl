#ifndef VISION_SAMPLING_GLSL
#define VISION_SAMPLING_GLSL

/**
 * @brief Bilinear fetch from a sub-rectangle of a storage image.
 *
 * Storage images have no hardware filtering, so subpixel sampling is done with
 * four imageLoad calls. Defined as a macro for the same reason as
 * sample_clamped: GLSL rejects a format-qualified image2D as a function
 * parameter.
 *
 * Coordinates are in pixel-index space: an integer position addresses a texel
 * exactly and 0.5 blends two neighbours equally. The position is clamped to the
 * rectangle, so the rectangle edge replicates outward and never reads a
 * neighbouring region of the same image.
 *
 * @param img     Storage image2D binding, any float format.
 * @param pos     vec2 position relative to origin, in pixels.
 * @param origin  ivec2 top-left texel of the rectangle within img.
 * @param extent  ivec2 size of the rectangle in texels.
 * @param out_val vec4 lvalue receiving the interpolated texel.
 */
#define SAMPLE_BILINEAR_REGION(img, pos, origin, extent, out_val) \
    do { \
        const ivec2 _ext = ivec2(extent); \
        const vec2 _p = clamp((pos), vec2(0.0), vec2(_ext) - vec2(1.0)); \
        const ivec2 _i0 = ivec2(floor(_p)); \
        const ivec2 _i1 = min(_i0 + ivec2(1), _ext - ivec2(1)); \
        const vec2 _f = _p - vec2(_i0); \
        const ivec2 _o = ivec2(origin); \
        const vec4 _a = imageLoad((img), _o + _i0); \
        const vec4 _b = imageLoad((img), _o + ivec2(_i1.x, _i0.y)); \
        const vec4 _c = imageLoad((img), _o + ivec2(_i0.x, _i1.y)); \
        const vec4 _d = imageLoad((img), _o + _i1); \
        (out_val) = mix(mix(_a, _b, _f.x), mix(_c, _d, _f.x), _f.y); \
    } while (false)

/**
 * @brief Bilinear fetch over a whole storage image, clamped to its edges.
 *
 * @param img     Storage image2D binding, any float format.
 * @param pos     vec2 position in pixels.
 * @param out_val vec4 lvalue receiving the interpolated texel.
 */
#define SAMPLE_BILINEAR(img, pos, out_val) \
    SAMPLE_BILINEAR_REGION((img), (pos), ivec2(0), imageSize(img), (out_val))

#endif // VISION_SAMPLING_GLSL
