#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_world_pos;
layout(location = 4) flat in uint in_texture;
layout(location = 5) flat in float in_scalar;

layout(set = 0, binding = 2) uniform sampler2D diffuseTex[];

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex = texture(diffuseTex[nonuniformEXT(in_texture)], in_uv);
    if (tex.a < 0.5) {
        discard;
    }
    out_color = vec4(tex.rgb * in_color, tex.a);
}
