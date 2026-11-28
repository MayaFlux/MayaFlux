#ifndef MF_RADIOMETRY_SOLID_ANGLE_GLSL
#define MF_RADIOMETRY_SOLID_ANGLE_GLSL

/**
 * Mirrors Kinesis/Radiometry/SolidAngle.hpp. Positions are relative to the
 * receiving point and irradiances are per unit radiance. Polygons are built in
 * the shared array radiometry_corners.
 */

#ifndef MF_PI
#define MF_PI 3.14159265358979323846
#endif

#define POLYGON_CORNER_LIMIT 16

vec3 radiometry_corners[POLYGON_CORNER_LIMIT];

/** Irradiance from a sphere, including the part cut off by the horizon. */
float sphere_irradiance(float cos_theta, float sin_sq)
{
    if (!(sin_sq > 0.0)) {
        return 0.0;
    }
    if (sin_sq >= 1.0) {
        return MF_PI;
    }
    float c = clamp(cos_theta, -1.0, 1.0);
    if (c * c > sin_sq) {
        return MF_PI * sin_sq * max(c, 0.0);
    }
    float sin_theta = sqrt(max(1.0 - c * c, 1e-12));
    float x = sqrt(1.0 / sin_sq - 1.0);
    float y = clamp(-x * c / sin_theta, -1.0, 1.0);
    float across = sin_theta * sqrt(max(1.0 - y * y, 0.0));
    return max((c * acos(y) - x * across) * sin_sq + atan(across / x), 0.0);
}

/**
 * Irradiance from the planar convex polygon held in the first count entries of
 * radiometry_corners, clipped to the horizon of the unit normal.
 */
float polygon_irradiance(int count, vec3 normal)
{
    vec3 kept[POLYGON_CORNER_LIMIT + 1];
    int held = 0;
    for (int i = 0; i < count; ++i) {
        vec3 a = radiometry_corners[i];
        vec3 b = radiometry_corners[(i + 1) % count];
        float above_a = dot(a, normal);
        float above_b = dot(b, normal);
        if (above_a > 0.0 && held <= POLYGON_CORNER_LIMIT) {
            kept[held++] = a;
        }
        if ((above_a > 0.0) != (above_b > 0.0) && held <= POLYGON_CORNER_LIMIT) {
            kept[held++] = a + (b - a) * (above_a / (above_a - above_b));
        }
    }
    if (held < 3) {
        return 0.0;
    }

    float sum = 0.0;
    for (int i = 0; i < held; ++i) {
        vec3 a = kept[i];
        vec3 b = kept[(i + 1) % held];
        vec3 axis = cross(a, b);
        float size = length(axis);
        if (!(size > 1e-12)) {
            continue;
        }
        sum += atan(size, dot(a, b)) * dot(axis / size, normal);
    }
    return 0.5 * abs(sum);
}

void rectangle_corners(vec3 center, vec3 right, vec3 up, vec2 size)
{
    vec3 w = right * (0.5 * size.x);
    vec3 h = up * (0.5 * size.y);
    radiometry_corners[0] = center - w - h;
    radiometry_corners[1] = center + w - h;
    radiometry_corners[2] = center + w + h;
    radiometry_corners[3] = center - w + h;
}

/** A sixteen sided polygon with the area of a disk. */
void disk_corners(vec3 center, vec3 right, vec3 up, float radius)
{
    float turn = MF_PI / 8.0;
    float reach = radius * sqrt(turn / sin(turn));
    for (int i = 0; i < 16; ++i) {
        float angle = turn * float(i);
        radiometry_corners[i] = center + (right * cos(angle) + up * sin(angle)) * reach;
    }
}

/** A tube as the rectangle of its length and diameter turned toward the point. */
void tube_corners(vec3 center, vec3 axis, float radius, float tube_length)
{
    vec3 side = cross(axis, center);
    float size = length(side);
    side = size > 1e-8 ? side / size : vec3(0.0);
    vec3 along = axis * (0.5 * tube_length);
    vec3 across = side * radius;
    radiometry_corners[0] = center - along - across;
    radiometry_corners[1] = center + along - across;
    radiometry_corners[2] = center + along + across;
    radiometry_corners[3] = center - along + across;
}

vec3 sphere_toward(vec3 center, float radius, vec3 ray)
{
    vec3 missed = ray * dot(center, ray) - center;
    float dist = length(missed);
    return dist > radius ? center + missed * (radius / dist) : center + missed;
}

vec3 segment_toward(vec3 from, vec3 to, vec3 ray)
{
    vec3 along = to - from;
    float ray_along = dot(ray, along);
    float divisor = dot(along, along) - ray_along * ray_along;
    float t = divisor > 1e-12
        ? clamp((dot(ray, from) * ray_along - dot(from, along)) / divisor, 0.0, 1.0)
        : 0.5;
    return from + along * t;
}

vec3 plane_toward(vec3 center, vec3 plane_normal, vec3 ray)
{
    float facing = dot(ray, plane_normal);
    if (abs(facing) < 1e-6) {
        return center;
    }
    float t = dot(center, plane_normal) / facing;
    return t > 0.0 ? ray * t : center;
}

vec3 rectangle_toward(vec3 center, vec3 right, vec3 up, vec2 size, vec3 ray)
{
    vec3 offset = plane_toward(center, cross(right, up), ray) - center;
    return center
        + right * clamp(dot(offset, right), -0.5 * size.x, 0.5 * size.x)
        + up * clamp(dot(offset, up), -0.5 * size.y, 0.5 * size.y);
}

vec3 disk_toward(vec3 center, vec3 plane_normal, float radius, vec3 ray)
{
    vec3 offset = plane_toward(center, plane_normal, ray) - center;
    float dist = length(offset);
    return dist > radius ? center + offset * (radius / dist) : center + offset;
}

/** Width of a lobe widened to cover a source of a size (Karis). */
float widened_alpha(float alpha, float radius, float dist)
{
    return clamp(alpha + radius / (2.0 * max(dist, 1e-4)), alpha, 1.0);
}

#endif
