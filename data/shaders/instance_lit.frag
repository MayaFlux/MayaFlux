#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_world_pos;

#include "include/influence_light.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 lit = in_color + influence_light(in_world_pos, in_normal);
    out_color = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
