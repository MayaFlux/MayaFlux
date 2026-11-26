#version 460
#extension GL_GOOGLE_include_directive : enable

#include "include/influence_facing.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) flat in uint in_slot;
layout(location = 4) in vec3 in_world_pos;

#include "include/influence_light.glsl"

layout(location = 0) out vec4 out_color;

void main()
{
    float reach = influence_reach(in_world_pos);

    vec3 lit = in_color + (color * intensity * reach * influence_facing(in_normal, position - in_world_pos));
    out_color = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
