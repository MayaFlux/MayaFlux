#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_world_pos;

#include "include/influence_light.glsl"

layout(location = 0) out vec4 outColor;

void main()
{
    float coverage = mill_coverage(in_uv);
    if (coverage <= 0.0) {
        discard;
    }

    float edge_softness = 0.1;
    float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
    float alpha = 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);

    vec3 lit = in_color + influence_light(in_world_pos, vec3(0.0));
    if (mill_is_wire(in_uv) || mill_is_point(in_uv)) {
        alpha = coverage;
    }
    outColor = vec4(clamp(lit, 0.0, 1.0), alpha);
}
