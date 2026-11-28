#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/lighting_influence.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) flat in uint in_slot;
layout(location = 4) in vec3 in_world_pos;

layout(set = 0, binding = 3) uniform sampler2D diffuseTex[];

layout(location = 8) flat in vec4 in_eye;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex = texture(diffuseTex[in_slot], in_uv);

    vec3 base = surface_texture_color(tex.rgb);
    out_color = vec4(influence_shade(base, in_world_pos, in_normal, in_eye), tex.a);
}
