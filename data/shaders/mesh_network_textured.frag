#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) flat in uint in_slot;

layout(set = 0, binding = 2) uniform sampler2D diffuseTex[];

layout(location = 0) out vec4 out_color;

void main()
{
    out_color = texture(diffuseTex[in_slot], in_uv);
}
