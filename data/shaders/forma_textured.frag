#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

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
    vec4 tex = texture(texSampler, sample_uv);
    out_color = vec4(in_color * tex.rgb, tex.a * coverage);
}
