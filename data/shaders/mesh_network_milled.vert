#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;
layout(location = 6) in float in_slot_low;
layout(location = 7) in float in_slot_high;

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} pc;

layout(set = 0, binding = 1) readonly buffer ModelMatrices {
    mat4 model[];
} models;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) flat out uint out_slot;
layout(location = 4) out vec3 out_world_pos;

void main()
{
    uint slot = uint(in_slot_low + 0.5) | (uint(in_slot_high + 0.5) << 16u);
    mat4 world_transform = pc.geometry * models.model[slot];
    mat3 normal_mat = transpose(inverse(mat3(world_transform)));

    vec4 world = world_transform * vec4(in_position, 1.0);
    gl_Position = pc.projection * pc.view * world;
    out_color = in_color;
    out_uv = in_uv;
    out_normal = normalize(normal_mat * in_normal);
    out_slot = slot;
    out_world_pos = world.xyz;
}
