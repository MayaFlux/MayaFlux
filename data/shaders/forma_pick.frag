#version 460

#include "include/mill_shape.glsl"

layout(constant_id = 8) const float PICK_ID = 0.0;

layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_value;

void main()
{
    if (mill_coverage(in_uv) <= 0.0) {
        discard;
    }

    out_value = vec4(PICK_ID, mill_surface_uv(in_uv), gl_FragCoord.z);
}
