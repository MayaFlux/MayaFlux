#ifndef MF_LIGHTING_INFLUENCE_GLSL
#define MF_LIGHTING_INFLUENCE_GLSL

/**
 * The influences of Nexus as lights, for the library's lit shaders. The surface
 * is set per renderer with specialization constants.
 */

#include "lighting.glsl"
#include "radiometry_tone.glsl"

layout(constant_id = 9) const float SURFACE_ROUGHNESS = 0.5;
layout(constant_id = 10) const float SURFACE_METALLIC = 0.0;
layout(constant_id = 11) const float SURFACE_DIFFUSE_ROUGHNESS = 0.0;
/** Whether colour textures hold sRGB values that were not decoded when sampled. */
layout(constant_id = 12) const bool SURFACE_TEXTURE_ENCODED = true;

/** One influence, as Nexus::InfluenceBlock mirrors it. */
struct InfluenceRecord {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
    vec3 direction;
    float cone_inner;
    vec3 attenuation;
    float cone_outer;
};

#ifdef MF_INFLUENCE_LIST
/** The buffer Nexus::InfluenceList fills. A place nothing holds is all zero. */
layout(set = 1, binding = 0, std430) readonly buffer Influences {
    uint influence_count;
    uint influence_unused0;
    uint influence_unused1;
    uint influence_unused2;
    InfluenceRecord influences[];
};
#else
/** The block Nexus::InfluenceBlock mirrors. */
layout(push_constant) uniform Influence {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
    vec3 direction;
    float cone_inner;
    vec3 attenuation;
    float cone_outer;
};
#endif

/**
 * The light an influence stands for: a spot where cone_outer is above zero,
 * otherwise a point. Cone angles are radians from the axis.
 */
Light influence_as_light(InfluenceRecord held)
{
    Light light = Light(held.position, held.intensity, held.color, held.radius,
        held.attenuation, vec3(0.0, 0.0, -1.0), 1.0, -1.0, LIGHT_POINT);
    if (held.cone_outer > 0.0) {
        light.kind = LIGHT_SPOT;
        light.direction = normalize(held.direction);
        light.cos_inner = cos(min(held.cone_inner, held.cone_outer));
        light.cos_outer = cos(held.cone_outer);
    }
    return light;
}

/** A colour texture sample as linear colour. */
vec3 surface_texture_color(vec3 sampled)
{
    return SURFACE_TEXTURE_ENCODED ? srgb_to_linear(sampled) : sampled;
}

/**
 * Displayable linear colour of a surface point lit by the influences. eye is
 * what view_eye() gives. A zero normal marks geometry without a surface
 * direction.
 */
vec3 influence_shade(vec3 base_color, vec3 at, vec3 normal, vec4 eye)
{
    SurfaceResponse surface = SurfaceResponse(
        base_color, SURFACE_ROUGHNESS, SURFACE_METALLIC, SURFACE_DIFFUSE_ROUGHNESS, 0.04);
    vec3 view = normalize(eye.xyz - at * eye.w);
    vec3 total = vec3(0.0);

#ifdef MF_INFLUENCE_LIST
    for (uint i = 0u; i < influence_count; ++i) {
        total += light_contribution(influence_as_light(influences[i]), surface, at, normal, view, 1.0);
    }
#else
    InfluenceRecord held = InfluenceRecord(
        position, intensity, color, radius, size, direction, cone_inner, attenuation, cone_outer);
    total = light_contribution(influence_as_light(held), surface, at, normal, view, 1.0);
#endif

    return pbr_neutral(max(total, vec3(0.0)));
}

#endif
