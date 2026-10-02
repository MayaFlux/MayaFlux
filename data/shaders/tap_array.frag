#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

/// @brief Tap layers. Bound at binding 1 as the buffer's default texture, a
///        VKImage created via TextureLoom::create_2d_array.
layout(set = 0, binding = 1) uniform sampler2DArray tapArray;

const uint MAX_LAYERS = 30u;

/// @brief layer_count: layers in use, at most MAX_LAYERS.
/// @brief mode: 0 weighted mean, 1 weighted sum, 2 weighted max, 3 over in layer order.
/// @brief weights: per layer weight. A layer with a weight of zero or less is skipped.
layout(push_constant) uniform PC {
    uint layer_count;
    uint mode;
    float weights[MAX_LAYERS];
} pc;

void main()
{
    vec4 accum = vec4(0.0);
    float total = 0.0;
    uint count = min(pc.layer_count, MAX_LAYERS);

    for (uint i = 0u; i < count; ++i) {
        float w = pc.weights[i];
        if (w <= 0.0) {
            continue;
        }

        vec4 c = texture(tapArray, vec3(fragTexCoord, float(i)));

        if (pc.mode == 0u) {
            accum += c * w;
            total += w;
        } else if (pc.mode == 1u) {
            accum += c * w;
        } else if (pc.mode == 2u) {
            accum = max(accum, c * w);
        } else {
            float a = c.a * w;
            accum = vec4(mix(accum.rgb, c.rgb, a), a + accum.a * (1.0 - a));
        }
    }

    if (pc.mode == 0u && total > 0.0) {
        accum /= total;
    }

    outColor = clamp(accum, 0.0, 1.0);
}
