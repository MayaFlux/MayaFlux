#ifndef MF_LIGHTING_GLSL
#define MF_LIGHTING_GLSL

/**
 * Direct lighting of a surface point by one light. Declares no bindings: sum
 * light_contribution() over whatever lights a shader holds, then add the
 * surface's emission once.
 */

#include "radiometry_attenuation.glsl"
#include "radiometry_reflectance.glsl"

#define LIGHT_POINT 0u
#define LIGHT_SPOT 1u
#define LIGHT_DIRECTIONAL 2u

struct Light {
    vec3 position;
    /** Radiant intensity for point and spot lights, irradiance for directional ones. */
    float intensity;
    /** Linear colour. */
    vec3 color;
    /** Distance at which point and spot lights reach zero. */
    float range;
    /**
     * Constant, linear and quadratic terms dividing point and spot lights with
     * distance. (0, 0, 1) is the inverse square; all zero is no division.
     */
    vec3 falloff;
    /** Unit direction the light travels, for spot and directional lights. */
    vec3 direction;
    float cos_inner;
    float cos_outer;
    uint kind;
};

/**
 * Irradiance the light delivers to a surface facing it at point at, and in
 * toward the unit direction from that point to the light. Zero, with a zero
 * direction, where the point coincides with the light.
 */
vec3 light_arrival(Light light, vec3 at, out vec3 toward)
{
    if (light.kind == LIGHT_DIRECTIONAL) {
        toward = -light.direction;
        return light.color * light.intensity;
    }

    vec3 offset = light.position - at;
    float dist = length(offset);
    if (!(dist > 1e-6)) {
        toward = vec3(0.0);
        return vec3(0.0);
    }
    toward = offset / dist;

    float divisor = dot(light.falloff, vec3(1.0, dist, dist * dist));
    float reach = range_window(dist, light.range);
    if (divisor > 0.0) {
        reach /= max(divisor, 1e-4);
    }
    if (light.kind == LIGHT_SPOT) {
        reach *= angular_window(dot(-toward, light.direction), light.cos_inner, light.cos_outer);
    }
    return light.color * (light.intensity * reach);
}

/**
 * Radiance one light adds toward view. A zero normal marks geometry without a
 * surface direction, such as points and lines: it receives from every side and
 * reflects diffusely.
 */
vec3 light_contribution(Light light, SurfaceResponse surface, vec3 at, vec3 normal, vec3 view, float specular_albedo)
{
    vec3 toward;
    vec3 arriving = light_arrival(light, at, toward);
    if (dot(toward, toward) == 0.0) {
        return vec3(0.0);
    }

    float normal_sq = dot(normal, normal);
    if (!(normal_sq > 1e-8)) {
        return surface.base_color * arriving / MF_PI;
    }

    vec3 unit = normal * inversesqrt(normal_sq);
    return surface_reflectance(surface, unit, view, toward, specular_albedo)
        * arriving * max(dot(unit, toward), 0.0);
}

#endif
