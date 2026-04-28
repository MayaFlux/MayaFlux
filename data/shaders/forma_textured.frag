#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex = texture(texSampler, in_uv);
    out_color = vec4(in_color * tex.rgb, tex.a);
}
