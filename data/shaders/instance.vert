#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} vt;

layout(set = 0, binding = 1) readonly buffer InstanceTransforms {
    mat4 transforms[];
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_world_pos;

void main()
{
    mat4 instance_transform = transforms[gl_InstanceIndex];
    mat4 world_transform = vt.geometry * instance_transform;
    mat3 normal_mat = transpose(inverse(mat3(world_transform)));
    vec4 world_pos = world_transform * vec4(in_position, 1.0);
    gl_Position = vt.projection * vt.view * world_pos;
    out_color = in_color;
    out_uv = in_uv;
    out_normal = normalize(normal_mat * in_normal);
    out_world_pos = world_pos.xyz;
}
