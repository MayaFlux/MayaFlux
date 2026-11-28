#ifndef MF_SHADOW_GLSL
#define MF_SHADOW_GLSL

/**
 * Filtering of a shadow map the shader is given. Maps hold depth in [0, 1],
 * smaller nearer the light. Every function returns how much of the light
 * arrives: 1 lit, 0 shadowed, and 1 outside the map.
 */

#define SHADOW_RADIUS_LIMIT 6

bool shadow_outside(vec2 uv, float depth)
{
    return any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || depth > 1.0;
}

float shadow_compare(sampler2D map, vec2 uv, float depth, float bias)
{
    if (shadow_outside(uv, depth)) {
        return 1.0;
    }
    return depth - bias <= textureLod(map, uv, 0.0).r ? 1.0 : 0.0;
}

/** Percentage closer filtering over a square of radius steps, spread texels apart. */
float shadow_pcf(sampler2D map, vec2 uv, float depth, float bias, int radius, float spread)
{
    int reach = clamp(radius, 0, SHADOW_RADIUS_LIMIT);
    vec2 apart = spread / vec2(textureSize(map, 0));
    float lit = 0.0;
    for (int y = -reach; y <= reach; ++y) {
        for (int x = -reach; x <= reach; ++x) {
            lit += shadow_compare(map, uv + vec2(x, y) * apart, depth, bias);
        }
    }
    float side = float(2 * reach + 1);
    return lit / (side * side);
}

/**
 * Contact hardening (Fernando): the penumbra widens with the distance from what
 * casts it. light_size is the light's width as a share of the map.
 */
float shadow_contact(sampler2D map, vec2 uv, float depth, float bias, float light_size)
{
    if (shadow_outside(uv, depth)) {
        return 1.0;
    }

    float blocked = 0.0;
    float count = 0.0;
    for (int y = -3; y <= 3; ++y) {
        for (int x = -3; x <= 3; ++x) {
            float held = textureLod(map, uv + vec2(x, y) * (light_size / 6.0), 0.0).r;
            if (held < depth - bias) {
                blocked += held;
                count += 1.0;
            }
        }
    }
    if (count == 0.0) {
        return 1.0;
    }

    float blocker = max(blocked / count, 1e-4);
    float penumbra = (depth - blocker) / blocker * light_size;
    float texels = penumbra * float(textureSize(map, 0).x);
    return shadow_pcf(map, uv, depth, bias, 4, max(texels / 8.0, 1.0));
}

/**
 * Variance shadow map (Donnelly and Lauritzen): the map holds depth in r and
 * its square in g, both filtered. bleed in [0, 1) trims light leaking through.
 */
float shadow_variance(sampler2D moments, vec2 uv, float depth, float least_variance, float bleed)
{
    if (shadow_outside(uv, depth)) {
        return 1.0;
    }
    vec2 held = texture(moments, uv).rg;
    if (depth <= held.x) {
        return 1.0;
    }
    float variance = max(held.y - held.x * held.x, least_variance);
    float apart = depth - held.x;
    float bound = variance / (variance + apart * apart);
    return clamp((bound - bleed) / max(1.0 - bleed, 1e-4), 0.0, 1.0);
}

#endif
