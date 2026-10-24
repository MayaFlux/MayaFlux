#version 460

#include "include/glyph_coverage.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;
layout(location = 3) in float in_weight;

layout(set = 0, binding = 1) uniform sampler2D textures[8];

layout(location = 0) out vec4 out_color;

void main()
{
    if (glyph_is_coverage(in_weight)) {
        float coverage = texture(textures[glyph_slot(in_weight)], in_uv).r;
        out_color = vec4(in_color, coverage);
        return;
    }

    out_color = vec4(in_color, 1.0);
}
