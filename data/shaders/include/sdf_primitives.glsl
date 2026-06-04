#ifndef SDF_PRIMITIVES_GLSL
#define SDF_PRIMITIVES_GLSL

// ---------------------------------------------------------------------------
// Signed distance primitives
// ---------------------------------------------------------------------------

/// @brief Sphere SDF centred at @p c with radius @p r.
float sdf_sphere(vec3 p, vec3 c, float r)
{
    return length(p - c) - r;
}

/// @brief Axis-aligned box SDF centred at @p c with half-extents @p b.
float sdf_box(vec3 p, vec3 c, vec3 b)
{
    vec3 q = abs(p - c) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

/// @brief Gyroid surface: zero-set approximates a triply-periodic minimal surface.
/// @param p  Evaluation point.
/// @param s  Spatial frequency.
/// @param t  Temporal phase offset (animate by incrementing each frame).
float sdf_gyroid(vec3 p, float s, float t)
{
    return sin(p.x * s + t) * cos(p.y * s)
        + sin(p.y * s + t * 0.7) * cos(p.z * s)
        + sin(p.z * s + t * 1.3) * cos(p.x * s);
}

// ---------------------------------------------------------------------------
// Boolean / blending operators
// ---------------------------------------------------------------------------

/// @brief Hard union: minimum of two distances.
float sdf_union(float a, float b) {
    return min(a, b);
}

/// @brief Hard subtraction: carve @p b from @p a.
float sdf_subtract(float a, float b) {
    return max(a, -b);
}

/// @brief Hard intersection.
float sdf_intersect(float a, float b) {
    return max(a, b);
}

/// @brief Smooth union (polynomial blend, k controls blend radius).
float sdf_smooth_union(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

/// @brief Smooth subtraction.
float sdf_smooth_subtract(float a, float b, float k)
{
    float h = clamp(0.5 - 0.5 * (b + a) / k, 0.0, 1.0);
    return mix(a, -b, h) + k * h * (1.0 - h);
}

/// @brief Smooth intersection.
float sdf_smooth_intersect(float a, float b, float k)
{
    float h = clamp(0.5 - 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) + k * h * (1.0 - h);
}

#endif // SDF_PRIMITIVES_GLSL
