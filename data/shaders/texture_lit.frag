#version 460

#include "include/lighting_influence.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 in_world_pos;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 8) flat in vec4 in_eye;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 tex = texture(texSampler, fragTexCoord);

    vec3 base = surface_texture_color(tex.rgb);
    outColor = vec4(influence_shade(base, in_world_pos, vec3(0.0), in_eye), tex.a);
}
