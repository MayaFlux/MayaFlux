#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;

layout(push_constant) uniform Influence {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
};

layout(location = 0) out vec4 out_color;

void main()
{
    float coverage = mill_coverage(in_uv);
    if (coverage <= 0.0) {
        discard;
    }

    float alpha = coverage;
    if (MILL_ACTIVE && !mill_is_wire(in_uv) && !mill_is_point(in_uv)) {
        float edge_softness = 0.1;
        float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
        alpha = 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);
    }

    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = in_color + (color * intensity * attenuation);
    out_color = vec4(clamp(lit, 0.0, 1.0), alpha);
}
