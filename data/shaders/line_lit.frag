#version 460

#include "include/lighting_influence.glsl"

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_world_pos;

layout(location = 8) flat in vec4 in_eye;

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

    if (mill_is_wire(in_uv) || mill_is_point(in_uv)) {
        alpha = coverage;
    }
    outColor = vec4(influence_shade(in_color, in_world_pos, vec3(0.0), in_eye), alpha);
}
