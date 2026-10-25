#version 460

#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;

layout(set = 0, binding = 1) uniform sampler2D diffuseTex;

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

    vec4 tex = vec4(in_color, coverage);
    if (!mill_is_wire(in_uv)) {
        vec2 sample_uv = mill_surface_uv(in_uv);
        if (mill_is_point(in_uv)) {
            sample_uv = mill_point_coord(in_uv);
        }
        tex = texture(diffuseTex, sample_uv);
        if (MILL_ACTIVE) {
            tex.rgb *= in_color;
            if (!mill_is_point(in_uv)) {
                float edge_softness = 0.1;
                float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
                tex.a *= 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);
            }
        }
    }

    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = tex.rgb + (color * intensity * attenuation);
    out_color = vec4(clamp(lit, 0.0, 1.0), tex.a);
}
