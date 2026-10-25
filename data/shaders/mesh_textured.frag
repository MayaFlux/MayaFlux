#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(set = 0, binding = 1) uniform sampler2D diffuseTex;

layout(location = 0) out vec4 out_color;

void main()
{
    float coverage = mill_coverage(in_uv);
    if (coverage <= 0.0) {
        discard;
    }

    if (mill_is_wire(in_uv)) {
        out_color = vec4(in_color, coverage);
        return;
    }

    vec2 sample_uv = mill_surface_uv(in_uv);
    if (mill_is_point(in_uv)) {
        sample_uv = mill_point_coord(in_uv);
    }
    vec4 sampled = texture(diffuseTex, sample_uv);

    if (MILL_ACTIVE) {
        float alpha = sampled.a;
        if (!mill_is_point(in_uv)) {
            float edge_softness = 0.1;
            float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
            alpha *= 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);
        }
        out_color = vec4(in_color * sampled.rgb, alpha);
        return;
    }

    out_color = sampled;
}
