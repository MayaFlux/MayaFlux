#ifndef MF_INFLUENCE_FACING_GLSL
#define MF_INFLUENCE_FACING_GLSL

/**
 * How squarely a surface faces an influence: 1 head on, 0 from behind, and 1 where the
 * surface carries no normal or sits at the influence itself.
 */
float influence_facing(vec3 normal, vec3 toward)
{
    float normal_sq = dot(normal, normal);
    float toward_sq = dot(toward, toward);
    if (!(normal_sq > 1e-8) || !(toward_sq > 1e-8)) {
        return 1.0;
    }
    return max(dot(normal, toward) / sqrt(normal_sq * toward_sq), 0.0);
}

#endif
