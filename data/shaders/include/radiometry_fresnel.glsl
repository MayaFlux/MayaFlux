#ifndef MF_RADIOMETRY_FRESNEL_GLSL
#define MF_RADIOMETRY_FRESNEL_GLSL

/** Mirrors Kinesis/Radiometry/Fresnel.hpp. */

/** Schlick's approximation from the reflectance at normal incidence. */
vec3 schlick_fresnel(vec3 f0, float cos_theta)
{
    float m = clamp(1.0 - cos_theta, 0.0, 1.0);
    float m2 = m * m;
    return f0 + (vec3(1.0) - f0) * (m2 * m2 * m);
}

/** Cosine weighted hemispherical average of schlick_fresnel(). */
vec3 schlick_average(vec3 f0)
{
    return (vec3(1.0) + 20.0 * f0) / 21.0;
}

/** Unpolarised dielectric reflectance; eta is the far index over the near one. */
float dielectric_fresnel(float cos_theta, float eta)
{
    float c = clamp(cos_theta, 0.0, 1.0);
    float sin_t_sq = (1.0 - c * c) / (eta * eta);
    if (sin_t_sq >= 1.0) {
        return 1.0;
    }
    float ct = sqrt(1.0 - sin_t_sq);
    float parallel = (eta * c - ct) / (eta * c + ct);
    float across = (c - eta * ct) / (c + eta * ct);
    return 0.5 * (parallel * parallel + across * across);
}

/** Unpolarised conductor reflectance for a complex relative index eta + i k. */
vec3 conductor_fresnel(float cos_theta, vec3 eta, vec3 k)
{
    float c = clamp(cos_theta, 0.0, 1.0);
    float c2 = c * c;
    float s2 = 1.0 - c2;

    vec3 t0 = eta * eta - k * k - s2;
    vec3 magnitude = sqrt(t0 * t0 + 4.0 * eta * eta * k * k);
    vec3 t1 = magnitude + c2;
    vec3 a = sqrt(max(0.5 * (magnitude + t0), vec3(0.0)));
    vec3 t2 = 2.0 * c * a;
    vec3 below = t1 + t2;
    vec3 across = mix(vec3(1.0), (t1 - t2) / below, greaterThan(below, vec3(0.0)));

    vec3 t3 = c2 * magnitude + s2 * s2;
    vec3 t4 = t2 * s2;
    vec3 under = t3 + t4;
    vec3 parallel = across * mix(vec3(1.0), (t3 - t4) / under, greaterThan(under, vec3(0.0)));

    return 0.5 * (parallel + across);
}

#endif
