#version 460
#extension GL_GOOGLE_include_directive : enable

#include "include/lighting_influence.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_world_pos;

layout(location = 8) flat in vec4 in_eye;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(influence_shade(in_color, in_world_pos, in_normal, in_eye), 1.0);
}
