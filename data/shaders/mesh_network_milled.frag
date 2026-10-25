#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main()
{
    if (mill_is_wire(in_uv)) {
        float coverage = mill_wire_coverage(in_uv);
        if (coverage <= 0.0)
            discard;
        out_color = vec4(in_color, coverage);
        return;
    }

    if (mill_is_point(in_uv)) {
        vec2 local_uv = mill_point_local_uv(in_uv) * 2.0 - 1.0;
        if (dot(local_uv, local_uv) > 1.0)
            discard;
    }

    out_color = vec4(in_color, 1.0);
}
