#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;
layout(location = 3) in float in_weight;

layout(set = 0, binding = 1) uniform sampler2D textures[8];

layout(location = 0) out vec4 out_color;

void main()
{
    int idx = int(in_weight);
    if (idx <= 0) {
        out_color = vec4(in_color, 1.0);
    } else {
        vec4 tex = texture(textures[idx - 1], in_uv);
        out_color = vec4(in_color * tex.rgb, tex.a);
    }
}
