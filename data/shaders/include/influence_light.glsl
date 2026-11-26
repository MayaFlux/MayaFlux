#ifndef MF_INFLUENCE_LIGHT_GLSL
#define MF_INFLUENCE_LIGHT_GLSL

#include "influence_facing.glsl"

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
 * What one influence adds at a point: the fall to zero at the radius, divided by the
 * constant, linear and quadratic attenuation terms, cut to the cone about the direction
 * when cone_outer is above zero, and weighed by how squarely the normal faces it. Cone
 * angles are radians from the axis.
 */
vec3 influence_from(InfluenceRecord held, vec3 at, vec3 normal)
{
    if (!(held.radius > 0.0)) {
        return vec3(0.0);
    }
    vec3 away = at - held.position;
    float dist = length(away);

    float reach = clamp(1.0 - (dist / held.radius), 0.0, 1.0);
    reach *= reach;

    float divisor = dot(held.attenuation, vec3(1.0, dist, dist * dist));
    if (divisor > 1e-8) {
        reach /= divisor;
    }

    if (held.cone_outer > 0.0 && dist > 1e-6) {
        float outer = cos(held.cone_outer);
        float inner = cos(min(held.cone_inner, held.cone_outer));
        float along = dot(away / dist, normalize(held.direction));
        reach *= clamp((along - outer) / max(inner - outer, 1e-4), 0.0, 1.0);
    }
    return held.color * held.intensity * reach * influence_facing(normal, -away);
}

/** What the influences add at a point. A zero normal faces every one of them. */
vec3 influence_light(vec3 at, vec3 normal)
{
#ifdef MF_INFLUENCE_LIST
    vec3 total = vec3(0.0);
    for (uint i = 0u; i < influence_count; ++i) {
        total += influence_from(influences[i], at, normal);
    }
    return total;
#else
    return influence_from(
        InfluenceRecord(position, intensity, color, radius, size, direction, cone_inner, attenuation, cone_outer),
        at, normal);
#endif
}

#endif
