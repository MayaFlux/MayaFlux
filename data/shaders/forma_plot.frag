#version 460

#include "include/glyph_coverage.glsl"
#include "include/mill_shape.glsl"

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;
layout(location = 3) in float in_weight;

layout(set = 0, binding = 1) uniform sampler2D textures[8];

layout(constant_id = 2) const float CLIP_MIN_X = 0.0;
layout(constant_id = 3) const float CLIP_MIN_Y = 0.0;
layout(constant_id = 4) const float CLIP_MAX_X = 0.0;
layout(constant_id = 5) const float CLIP_MAX_Y = 0.0;
layout(constant_id = 6) const float ALPHA_CUTOFF = 0.0;

layout(location = 0) out vec4 out_color;

/// @brief Red channel of text slot @p slot. The slot differs between the
///        primitives of one draw, so each is read through a constant index,
///        and at level zero since an atlas has no other.
float glyph_texel(int slot, vec2 uv)
{
    switch (slot) {
    case 1:
        return textureLod(textures[1], uv, 0.0).r;
    case 2:
        return textureLod(textures[2], uv, 0.0).r;
    case 3:
        return textureLod(textures[3], uv, 0.0).r;
    default:
        return textureLod(textures[0], uv, 0.0).r;
    }
}

void main()
{
    bool clipped = CLIP_MAX_X > CLIP_MIN_X
        && (in_world_pos.x < CLIP_MIN_X || in_world_pos.x > CLIP_MAX_X
            || in_world_pos.y < CLIP_MIN_Y || in_world_pos.y > CLIP_MAX_Y);

    float coverage = mill_coverage(in_uv);
    if (clipped || coverage <= 0.0) {
        discard;
    }

    if (!mill_is_wire(in_uv) && glyph_is_coverage(in_weight)) {
        float glyph = glyph_texel(glyph_slot(in_weight), mill_surface_uv(in_uv));
        if (glyph * coverage < ALPHA_CUTOFF) {
            discard;
        }
        out_color = vec4(in_color, glyph * coverage);
        return;
    }

    if (coverage < ALPHA_CUTOFF) {
        discard;
    }

    out_color = vec4(in_color, coverage);
}
