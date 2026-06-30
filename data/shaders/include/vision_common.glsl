#ifndef VISION_COMMON_GLSL
#define VISION_COMMON_GLSL

// Standard vision shader binding layout:
//   binding 0 — writeonly rgba32f image2D  out_image  (output storage image)
//   binding 1 — readonly  rgba32f image2D  in_image   (input storage image)
// Additional bindings declared per-shader after these two.
// All vision shaders use rgba32f to match the assembled shader path in
// AsmGenerator. GLSL does not allow format qualifiers on function parameters,
// so image access helpers are macros rather than functions.

/**
 * @brief Current thread's texel coordinate from GlobalInvocationID.
 */
ivec2 thread_coord()
{
    return ivec2(gl_GlobalInvocationID.xy);
}

/**
 * @brief Clamp coord to [0, sz-1] and load from img.
 *
 * Defined as a macro because GLSL does not permit format qualifiers on
 * image2D function parameters — passing a rgba32f image2D to a plain
 * image2D parameter is a compile error under glslc --target-env=vulkan1.3.
 *
 * @param img   rgba32f image2D binding (readonly).
 * @param coord ivec2 texel coordinate, may be out of bounds.
 */
#define sample_clamped(img, coord) \
    imageLoad((img), clamp((coord), ivec2(0), imageSize(img) - ivec2(1)))

/**
 * @brief Store val into img at coord only when coord is in bounds.
 *
 * Defined as a macro for the same reason as sample_clamped.
 *
 * @param img   rgba32f image2D binding (writeonly).
 * @param coord ivec2 texel coordinate.
 * @param val   vec4 value to write.
 */
#define write_safe(img, coord, val) \
    do { \
        ivec2 _sz = imageSize(img); \
        if ((coord).x < _sz.x && (coord).y < _sz.y) \
            imageStore((img), (coord), (val)); \
    } while (false)

#endif // VISION_COMMON_GLSL
