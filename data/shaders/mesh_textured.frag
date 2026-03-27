#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(set = 0, binding = 0) uniform sampler2D diffuseTex;

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(diffuseTex, in_uv);
}
