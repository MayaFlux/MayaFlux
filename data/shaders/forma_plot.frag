#version 460

#include "include/glyph_coverage.glsl"
#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;
layout(location = 3) in float in_weight;

layout(set = 0, binding = 1) uniform sampler2D textures[8];

layout(location = 0) out vec4 out_color;

void main()
{
    float coverage = mill_coverage(in_uv);
    if (coverage <= 0.0) {
        discard;
    }

    if (!mill_is_wire(in_uv) && glyph_is_coverage(in_weight)) {
        float glyph = texture(textures[glyph_slot(in_weight)], mill_surface_uv(in_uv)).r;
        out_color = vec4(in_color, glyph * coverage);
        return;
    }

    out_color = vec4(in_color, coverage);
}
