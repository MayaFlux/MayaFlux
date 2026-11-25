#version 460
#extension GL_GOOGLE_include_directive : enable

#include "include/influence_facing.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_world_pos;

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
    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = in_color + (color * intensity * attenuation * influence_facing(in_normal, position - in_world_pos));
    out_color = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
