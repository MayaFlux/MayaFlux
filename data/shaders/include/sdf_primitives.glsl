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

// ---------------------------------------------------------------------------
// Generating families
// Each one covers a range of shapes through its parameters.
// ---------------------------------------------------------------------------

/// @brief Distance to the line segment from @p a to @p b.
/// Subtract a radius for a capsule. With @p a equal to @p b it is a point.
float sdf_segment(vec3 p, vec3 a, vec3 b)
{
    vec3 ab = b - a;
    float len2 = dot(ab, ab);
    float h = len2 > 0.0 ? clamp(dot(p - a, ab) / len2, 0.0, 1.0) : 0.0;
    return length(p - a - ab * h);
}

/// @brief Signed distance to the plane with unit normal @p n at offset @p d along it.
float sdf_plane(vec3 p, vec3 n, float d)
{
    return dot(p, n) - d;
}

/// @brief Anisotropic Lp ball centred at @p c with per-axis radii @p r.
/// Power 2 is an ellipsoid, 1 an octahedron, and a larger power approaches a box.
/// The power is clamped to [1, 32]. The value is implicit, not an exact distance.
float sdf_lp_ball(vec3 p, vec3 c, vec3 r, float power)
{
    float k = clamp(power, 1.0, 32.0);
    vec3 q = abs((p - c) / r);
    float n = pow(pow(q.x, k) + pow(q.y, k) + pow(q.z, k), 1.0 / k);
    return (n - 1.0) * min(r.x, min(r.y, r.z));
}

/// @brief Triply periodic surface selected by @p family.
/// 0 is the gyroid, 1 the Schwarz P, 2 the diamond, and fractions morph between neighbours.
/// Only the zero set is comparable between families, since their amplitudes differ.
/// @param p      Evaluation point.
/// @param s      Spatial frequency.
/// @param family Position along the family, clamped to [0, 2].
float sdf_periodic(vec3 p, float s, float family)
{
    vec3 q = p * s;
    vec3 sn = sin(q);
    vec3 cs = cos(q);
    float gyroid = sn.x * cs.y + sn.y * cs.z + sn.z * cs.x;
    float schwarz = cs.x + cs.y + cs.z;
    float diamond = sn.x * sn.y * sn.z + sn.x * cs.y * cs.z
        + cs.x * sn.y * cs.z + cs.x * cs.y * sn.z;
    float f = clamp(family, 0.0, 2.0);
    return f < 1.0 ? mix(gyroid, schwarz, f) : mix(schwarz, diamond, f - 1.0);
}

/// @brief Compact radial kernel: 1 at @p c, falling smoothly to 0 at @p radius.
/// Sum several and subtract a threshold, negated so inside is negative, for metaballs.
float sdf_kernel(vec3 p, vec3 c, float radius)
{
    vec3 d = p - c;
    float w = 1.0 - clamp(dot(d, d) / (radius * radius), 0.0, 1.0);
    return w * w * w;
}

// ---------------------------------------------------------------------------
// Space operators
// Each returns the point to evaluate a shape at, so any shape can follow.
// ---------------------------------------------------------------------------

/// @brief Rotate the point @p p about the unit axis @p axis by @p angle radians.
/// Evaluating a shape at the rotated point turns the shape by the opposite angle.
vec3 sdf_rotate(vec3 p, vec3 axis, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return p * c + cross(axis, p) * s + axis * dot(axis, p) * (1.0 - c);
}

/// @brief Mirror across the coordinate planes of the axes whose @p mask component is 1.
vec3 sdf_mirror(vec3 p, vec3 mask)
{
    return mix(p, abs(p), mask);
}

/// @brief Infinite repetition with @p period along each axis.
/// A zero component leaves that axis unrepeated.
vec3 sdf_repeat(vec3 p, vec3 period)
{
    vec3 on = vec3(greaterThan(period, vec3(0.0)));
    vec3 safe = max(period, vec3(1e-6));
    return mix(p, p - safe * round(p / safe), on);
}

/// @brief Repeat around the y axis in @p count equal sectors.
vec3 sdf_radial_repeat(vec3 p, float count)
{
    float sector = 6.28318530718 / max(count, 1.0);
    float r = length(p.xz);
    float a = r > 1e-6 ? atan(p.z, p.x) : 0.0;
    float m = a - sector * floor(a / sector + 0.5);
    return vec3(r * cos(m), p.y, r * sin(m));
}

/// @brief Twist about the y axis by @p k radians per unit height.
vec3 sdf_twist(vec3 p, float k)
{
    float a = k * p.y;
    float c = cos(a);
    float s = sin(a);
    return vec3(c * p.x - s * p.z, p.y, s * p.x + c * p.z);
}

// ---------------------------------------------------------------------------
// Value operators
// Each reshapes the value of a field, so any field can be passed through.
// ---------------------------------------------------------------------------

/// @brief Hollow shell of half-thickness @p thickness around the zero set of @p d.
float sdf_shell(float d, float thickness)
{
    return abs(d) - thickness;
}

/// @brief Nested shells of @p d, @p spacing apart, each of half-thickness @p thickness.
float sdf_contours(float d, float spacing, float thickness)
{
    float h = 0.5 * spacing;
    return abs(mod(d + h, spacing) - h) - thickness;
}

/// @brief Linear morph from field @p a to field @p b as @p t goes from 0 to 1.
float sdf_morph(float a, float b, float t)
{
    return mix(a, b, t);
}

#endif // SDF_PRIMITIVES_GLSL
