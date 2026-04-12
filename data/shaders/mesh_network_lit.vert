#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;

layout(set = 0, binding = 0) uniform ViewTransformBlock {
    mat4 view;
    mat4 projection;
} pc;

layout(set = 0, binding = 1) readonly buffer ModelMatrices {
    mat4 model[];
} models;

layout(set = 0, binding = 2) readonly buffer SlotIndices {
    uint slot_index[];
} slots;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) flat out uint out_slot;
layout(location = 4) out vec3 out_world_pos;

void main()
{
    uint slot = slots.slot_index[gl_VertexIndex];
    mat4 model = models.model[slot];
    mat4 norm_mat = transpose(inverse(model));

    vec4 world = model * vec4(in_position, 1.0);
    gl_Position = pc.projection * pc.view * world;
    out_color = in_color;
    out_uv = in_uv;
    out_normal = normalize(mat3(norm_mat) * in_normal);
    out_slot = slot;
    out_world_pos = world.xyz;
}
