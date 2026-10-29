#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) flat in uint in_slot;

layout(set = 0, binding = 3) uniform sampler2D diffuseTex[];

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

    out_color = texture(diffuseTex[in_slot], mill_surface_uv(in_uv));
}
