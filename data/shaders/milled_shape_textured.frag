#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 1) uniform sampler2D tex;

layout(location = 0) out vec4 out_color;

void main()
{
    if (mill_is_point(in_uv)) {
        vec2 local = mill_point_local_uv(in_uv);
        if (length(local - vec2(0.5)) > 0.5) {
            discard;
        }
        vec4 sampled = texture(tex, local);
        out_color = vec4(in_color * sampled.rgb, sampled.a);
        return;
    }

    float edge_softness = 0.1;
    float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
    float edge_alpha = 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);

    vec4 sampled = texture(tex, in_uv);
    out_color = vec4(in_color * sampled.rgb, sampled.a * edge_alpha);
}
