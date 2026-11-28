#ifndef MF_LIGHTING_GLSL
#define MF_LIGHTING_GLSL

/**
 * Direct lighting of a surface point by one light. Declares no bindings: sum
 * light_contribution() over whatever lights a shader holds, then add the
 * surface's emission once.
 */

#include "radiometry_attenuation.glsl"
#include "radiometry_reflectance.glsl"
#include "radiometry_solid_angle.glsl"

#define LIGHT_POINT 0u
#define LIGHT_SPOT 1u
#define LIGHT_DIRECTIONAL 2u
#define LIGHT_SPHERE 3u
#define LIGHT_DISK 4u
#define LIGHT_RECTANGLE 5u
#define LIGHT_TUBE 6u

struct Light {
    vec3 position;
    /**
     * Radiant intensity for point and spot lights, irradiance for directional
     * ones, radiance for sphere, disk, rectangle and tube lights.
     */
    float intensity;
    /** Linear colour. */
    vec3 color;
    /** Distance at which placed lights reach zero. */
    float range;
    /**
     * Constant, linear and quadratic terms dividing point and spot lights with
     * distance. (0, 0, 1) is the inverse square; all zero is no division.
     */
    vec3 falloff;
    /**
     * Unit direction the light travels, for spot and directional lights, and the
     * side disk and rectangle lights shine from.
     */
    vec3 direction;
    float cos_inner;
    float cos_outer;
    uint kind;
    /** Unit width direction of a rectangle light, axis of a tube light. */
    vec3 right;
    /** Radius of a sphere or disk; width and height of a rectangle; radius and length of a tube. */
    vec2 extent;
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
 * How much of the surroundings' specular reflection survives an ambient
 * visibility (Lagarde). Mirrors Kinesis/Radiometry/Visibility.hpp.
 */
float specular_occlusion(float cos_view, float visibility, float roughness)
{
    float power = exp2(-16.0 * roughness - 1.0);
    return clamp(pow(max(cos_view + visibility, 0.0), power) - 1.0 + visibility, 0.0, 1.0);
}

/**
 * Radiance the surroundings add toward view, from the irradiance on the surface,
 * the radiance blurred to its roughness about the mirror direction, and the
 * specular albedo scale and bias. The normal is unit length. visibility is the
 * ambient visibility of the point, 1 in the open.
 */
vec3 environment_contribution(
    SurfaceResponse surface, vec3 normal, vec3 view, vec3 irradiance, vec3 reflected, vec2 albedo,
    float visibility)
{
    float cos_view = max(dot(normal, view), 0.0);
    vec3 entering = vec3(1.0) - schlick_fresnel(vec3(surface.dielectric_f0), cos_view);
    vec3 diffuse = entering * surface.base_color * irradiance * ((1.0 - surface.metallic) / MF_PI);
    vec3 f0 = surface_f0(surface);
    vec3 specular = reflected * (f0 * albedo.x + albedo.y)
        * multiple_scattering_compensation(f0, albedo.x + albedo.y);
    return diffuse * visibility + specular * specular_occlusion(cos_view, visibility, surface.roughness);
}

/**
 * What a shader holding the fitted lobe supplies to area lights: the inverse
 * matrix of linearly transformed cosines, and the specular albedo scale and bias
 * for the view. Absent, highlights use the nearest point of the light.
 */
struct LobeFit {
    bool present;
    mat3 inverse_lobe;
    vec2 albedo;
};

/**
 * Share of the fitted lobe covered by the polygon in radiometry_corners, which
 * this bends in place.
 */
float transformed_coverage(mat3 inverse_lobe, vec3 normal, vec3 view, int count)
{
    vec3 along = view - normal * dot(view, normal);
    float size = length(along);
    if (size > 1e-6) {
        along /= size;
    } else {
        vec3 other = abs(normal.x) < 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
        along = normalize(cross(normal, other));
    }
    mat3 bend = inverse_lobe * transpose(mat3(along, cross(normal, along), normal));
    for (int i = 0; i < count; ++i) {
        radiometry_corners[i] = bend * radiometry_corners[i];
    }
    return polygon_irradiance(count, vec3(0.0, 0.0, 1.0)) / MF_PI;
}

/**
 * Radiance an area light adds toward view. The diffuse part is exact; the
 * specular part takes the nearest point of the light to the mirror direction,
 * and is held to what a mirror would show.
 */
vec3 area_light_contribution(
    Light light, SurfaceResponse surface, vec3 at, vec3 normal, vec3 view, float specular_albedo, LobeFit fit)
{
    vec3 center = light.position - at;
    float dist = length(center);
    if (!(dist > 1e-6)) {
        return vec3(0.0);
    }

    bool planar = light.kind == LIGHT_DISK || light.kind == LIGHT_RECTANGLE;
    if (planar && !(dot(center, light.direction) < 0.0)) {
        return vec3(0.0);
    }

    vec3 radiance = light.color * (light.intensity * range_window(dist, light.range));
    vec3 right = light.right;
    vec3 up = planar ? cross(right, light.direction) : vec3(0.0);

    float radius = light.extent.x;
    float area = MF_PI * radius * radius;
    if (light.kind == LIGHT_RECTANGLE) {
        area = light.extent.x * light.extent.y;
        radius = sqrt(area / MF_PI);
    } else if (light.kind == LIGHT_TUBE) {
        area = 2.0 * light.extent.x * light.extent.y;
    }

    float normal_sq = dot(normal, normal);
    if (!(normal_sq > 1e-8)) {
        float irradiance = min(area / (dist * dist), MF_PI);
        return surface.base_color * radiance * (irradiance / MF_PI);
    }
    vec3 unit = normal * inversesqrt(normal_sq);
    vec3 mirrored = reflect(-view, unit);
    vec3 toward = center / dist;

    float irradiance;
    vec3 nearest;
    int corners = 0;
    if (light.kind == LIGHT_SPHERE) {
        irradiance = sphere_irradiance(dot(unit, toward), radius * radius / (dist * dist));
        nearest = sphere_toward(center, radius, mirrored);
    } else if (light.kind == LIGHT_DISK) {
        disk_corners(center, right, up, radius);
        corners = 16;
        irradiance = polygon_irradiance(16, unit);
        nearest = disk_toward(center, light.direction, radius, mirrored);
    } else if (light.kind == LIGHT_RECTANGLE) {
        rectangle_corners(center, right, up, light.extent);
        corners = 4;
        irradiance = polygon_irradiance(4, unit);
        nearest = rectangle_toward(center, right, up, light.extent, mirrored);
    } else {
        tube_corners(center, right, light.extent.x, light.extent.y);
        corners = 4;
        irradiance = polygon_irradiance(4, unit);
        vec3 along = right * (0.5 * light.extent.y);
        nearest = sphere_toward(segment_toward(center - along, center + along, mirrored), radius, mirrored);
    }
    if (!(irradiance > 0.0)) {
        return vec3(0.0);
    }

    float least = 0.5 * min(radius / dist, 1.0);
    vec3 seen = normalize(toward + unit * max(least - dot(unit, toward), 0.0));
    vec3 diffuse = surface_diffuse(surface, unit, view, seen) * irradiance;

    if (fit.present && !(light.kind == LIGHT_SPHERE && dist <= radius)) {
        if (light.kind == LIGHT_SPHERE) {
            vec3 side = abs(toward.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            vec3 across = normalize(cross(side, toward));
            float silhouette = radius * dist / sqrt(dist * dist - radius * radius);
            disk_corners(center, across, cross(toward, across), silhouette);
            corners = 16;
        }
        vec3 f0 = surface_f0(surface);
        vec3 bent = (f0 * fit.albedo.x + fit.albedo.y)
            * multiple_scattering_compensation(f0, fit.albedo.x + fit.albedo.y)
            * transformed_coverage(fit.inverse_lobe, unit, view, corners);
        return (diffuse + bent) * radiance;
    }

    float reach = length(nearest);
    vec3 source = reach > 1e-6 ? nearest / reach : toward;
    float alpha = roughness_to_alpha(surface.roughness);
    float wide = widened_alpha(alpha, radius, dist);
    float ratio = alpha / wide;
    float energy = light.kind == LIGHT_TUBE ? ratio : ratio * ratio;

    vec3 specular = surface_specular(surface, unit, view, source, alpha, specular_albedo) * (energy * irradiance);
    vec3 mirror = schlick_fresnel(surface_f0(surface), max(dot(unit, view), 0.0));

    return diffuse * radiance + min(specular, mirror) * radiance;
}

/**
 * Radiance one light adds toward view. A zero normal marks geometry without a
 * surface direction, such as points and lines: it receives from every side and
 * reflects diffusely.
 */
vec3 light_contribution(
    Light light, SurfaceResponse surface, vec3 at, vec3 normal, vec3 view, float specular_albedo, LobeFit fit)
{
    if (light.kind >= LIGHT_SPHERE) {
        return area_light_contribution(light, surface, at, normal, view, specular_albedo, fit);
    }

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
