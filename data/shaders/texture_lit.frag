#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 in_world_pos;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

#include "include/influence_light.glsl"

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 tex = texture(texSampler, fragTexCoord);

    vec3 lit = tex.rgb + influence_light(in_world_pos, vec3(0.0));
    outColor = vec4(clamp(lit, 0.0, 1.0), tex.a);
}
