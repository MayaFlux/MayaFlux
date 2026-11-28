#version 460

#include "include/view_eye.glsl"

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} vt;

layout(set = 0, binding = 1) readonly buffer InstanceTransforms {
    mat4 transforms[];
};

struct InstancePayload {
    vec3 tint;
    float scalar;
    uint texture;
    uint joint_count;
    uint target_count;
    uint vertex_count;
};

layout(std430, set = 0, binding = 8) readonly buffer InstancePayloads {
    InstancePayload payloads[];
};

struct Influence {
    uvec4 joints;
    vec4 weights;
};

layout(std430, set = 0, binding = 9) readonly buffer SkinInfluences {
    Influence influences[];
};

layout(std430, set = 0, binding = 10) readonly buffer MorphDeltas {
    float deltas[];
};

layout(std430, set = 0, binding = 11) readonly buffer SkinPalettes {
    mat4 palettes[];
};

layout(std430, set = 0, binding = 12) readonly buffer MorphWeights {
    float morph_weights[];
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec4 in_tangent;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_world_pos;
layout(location = 4) flat out uint out_texture;
layout(location = 5) flat out float out_scalar;

vec3 delta_at(uint base)
{
    return vec3(deltas[base], deltas[base + 1], deltas[base + 2]);
}
layout(location = 8) flat out vec4 out_eye;

void main()
{
    InstancePayload payload = payloads[gl_InstanceIndex];

    vec3 position = in_position;
    vec3 normal = in_normal;

    for (uint t = 0; t < payload.target_count; ++t) {
        float weight = morph_weights[gl_InstanceIndex * payload.target_count + t];
        uint base = (t * payload.vertex_count + gl_VertexIndex) * 9;
        position += weight * delta_at(base);
        normal += weight * delta_at(base + 3);
    }

    if (payload.joint_count > 0) {
        Influence influence = influences[gl_VertexIndex];
        uint first = gl_InstanceIndex * payload.joint_count;
        mat4 skin = (1.0 - dot(influence.weights, vec4(1.0))) * mat4(1.0);
        for (int c = 0; c < 4; ++c) {
            if (influence.weights[c] != 0.0) {
                skin += influence.weights[c] * palettes[first + influence.joints[c]];
            }
        }
        position = (skin * vec4(position, 1.0)).xyz;
        mat3 linear = mat3(skin);
        normal = mat3(cross(linear[1], linear[2]), cross(linear[2], linear[0]), cross(linear[0], linear[1])) * normal;
    }

    mat4 world_transform = vt.geometry * transforms[gl_InstanceIndex];
    mat3 normal_mat = transpose(inverse(mat3(world_transform)));
    vec4 world_pos = world_transform * vec4(position, 1.0);
    gl_Position = vt.projection * vt.view * world_pos;
    out_color = in_color * payload.tint;
    out_texture = payload.texture;
    out_scalar = payload.scalar;
    out_uv = in_uv;
    out_normal = normalize(normal_mat * normal);
    out_world_pos = world_pos.xyz;
    out_eye = view_eye(vt.view, vt.projection);
}
