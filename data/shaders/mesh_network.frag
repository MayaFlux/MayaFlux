#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

void main()
{
    float coverage = mill_coverage(in_uv);
    if (coverage <= 0.0)
        discard;

    if (mill_is_wire(in_uv)) {
        out_color = vec4(in_color, coverage);
        return;
    }

    out_color = vec4(in_color, 1.0);
}
