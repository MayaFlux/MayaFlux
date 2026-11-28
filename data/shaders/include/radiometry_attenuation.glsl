#ifndef MF_RADIOMETRY_ATTENUATION_GLSL
#define MF_RADIOMETRY_ATTENUATION_GLSL

/** Mirrors Kinesis/Radiometry/Attenuation.hpp. */

#ifndef MF_PI
#define MF_PI 3.14159265358979323846
#endif

/** Inverse square of a distance, held constant inside minimum_distance. */
float inverse_square_falloff(float dist, float minimum_distance)
{
    return 1.0 / max(dist * dist, minimum_distance * minimum_distance);
}

/** saturate(1 - (d/r)^4)^2, zero when range is not above zero. */
float range_window(float dist, float range)
{
    if (!(range > 0.0)) {
        return 0.0;
    }
    float ratio = dist / range;
    float squared = ratio * ratio;
    float window = clamp(1.0 - squared * squared, 0.0, 1.0);
    return window * window;
}

/** One inside the inner cone, zero outside the outer; arguments are cosines. */
float angular_window(float cos_angle, float cos_inner, float cos_outer)
{
    float t = clamp((cos_angle - cos_outer) / max(cos_inner - cos_outer, 1e-4), 0.0, 1.0);
    return t * t;
}

/** Reciprocal of terms.x + terms.y * d + terms.z * d^2, one when that is not above zero. */
float polynomial_falloff(float dist, vec3 terms)
{
    float divisor = terms.x + dist * (terms.y + dist * terms.z);
    return divisor > 1e-8 ? 1.0 / divisor : 1.0;
}

float falloff_linear(float t)
{
    return 1.0 - clamp(t, 0.0, 1.0);
}

float falloff_cosine(float t)
{
    return 0.5 * (1.0 + cos(clamp(t, 0.0, 1.0) * MF_PI));
}

float falloff_exponential(float t)
{
    return exp(-4.0 * clamp(t, 0.0, 1.0));
}

float falloff_inverse_square(float t)
{
    float c = clamp(t, 0.0, 1.0);
    return 1.0 / (1.0 + 16.0 * c * c);
}

#endif
