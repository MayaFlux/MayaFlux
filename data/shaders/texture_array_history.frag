#version 460

#include "include/history_ring.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2DArray textureArray;

#extension GL_EXT_buffer_reference : require

/// @brief Per layer weights, in a host visible buffer the array owns.
layout(buffer_reference, std430) readonly buffer LayerWeights {
    float value[];
};

/// @brief layer_count: layers in the ring, with no upper limit but the array's.
/// @brief mode: 0 faded mean, 1 faded maximum, 2 minimum of the frames read.
/// @brief weights.value[0]: fade per frame of age; 0 reads every frame at full weight.
/// @brief weights.value[1]: share of the ring read, newest first, in (0, 1]; 0 reads all of it.
/// @brief head: the layer the next frame is written to.
layout(push_constant) uniform PC {
    LayerWeights weights;
    uint layer_count;
    uint mode;
    uint head;
} pc;

void main()
{
    uint count = max(pc.layer_count, 1u);
    float share = pc.weights.value[1] > 0.0 ? min(pc.weights.value[1], 1.0) : 1.0;
    uint depth = clamp(uint(ceil(share * float(count))), 1u, count);
    float fade = max(pc.weights.value[0], 0.0);

    vec4 accum = vec4(0.0);
    vec4 strongest = vec4(0.0);
    vec4 weakest = vec4(1.0);
    float total = 0.0;

    for (uint age = 0u; age < depth; ++age) {
        vec4 frame = texture(textureArray, vec3(fragTexCoord, float(ring_layer(age, pc.head, count))));
        float w = exp(-fade * float(age));

        accum += frame * w;
        total += w;
        strongest = max(strongest, frame * w);
        weakest = min(weakest, frame);
    }

    if (pc.mode == 1u) {
        outColor = strongest;
    } else if (pc.mode == 2u) {
        outColor = weakest;
    } else {
        outColor = accum / max(total, 1e-6);
    }
}
