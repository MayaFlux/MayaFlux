#ifndef MF_INFLUENCE_LIGHT_GLSL
#define MF_INFLUENCE_LIGHT_GLSL

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

/**
 * How much of the influence arrives at a point: the fall to zero at the radius, divided by
 * the constant, linear and quadratic attenuation terms, and cut to the cone about the
 * direction when cone_outer is above zero. Cone angles are radians from the axis.
 */
float influence_reach(vec3 at)
{
    vec3 away = at - position;
    float dist = length(away);

    float reach = clamp(1.0 - (dist / radius), 0.0, 1.0);
    reach *= reach;

    float divisor = dot(attenuation, vec3(1.0, dist, dist * dist));
    if (divisor > 1e-8) {
        reach /= divisor;
    }

    if (cone_outer > 0.0 && dist > 1e-6) {
        float outer = cos(cone_outer);
        float inner = cos(min(cone_inner, cone_outer));
        float along = dot(away / dist, normalize(direction));
        reach *= clamp((along - outer) / max(inner - outer, 1e-4), 0.0, 1.0);
    }
    return reach;
}

#endif
