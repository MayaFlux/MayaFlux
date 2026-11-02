#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

/// @brief Array layers. Bound at binding 1 as the buffer's default texture, a
///        VKImage created via TextureLoom::create_2d_array.
layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

#extension GL_EXT_buffer_reference : require

/// @brief Per layer weights, in a host visible buffer the array owns.
layout(buffer_reference, std430) readonly buffer LayerWeights {
    float value[];
};

/// @brief weights: device address of the per layer weights. A layer with a weight of zero or less is skipped.
/// @brief layer_count: layers in use.
/// @brief mode: 0 weighted mean, 1 weighted sum, 2 weighted max, 3 over in layer order.
layout(push_constant) uniform PC {
    LayerWeights weights;
    uint layer_count;
    uint mode;
} pc;

void main()
{
    vec4 accum = vec4(0.0);
    float total = 0.0;
    uint count = pc.layer_count;

    for (uint i = 0u; i < count; ++i) {
        float w = pc.weights.value[i];
        if (w <= 0.0) {
            continue;
        }

        vec4 c = texture(textureArray, vec3(fragTexCoord, float(i)));

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
