#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} pc;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_world_pos;

void main()
{
    vec4 world_pos = pc.geometry * vec4(in_position, 1.0);
    gl_Position = pc.projection * pc.view * world_pos;
    out_color = in_color;
    out_uv = in_uv;
    out_world_pos = world_pos.xyz;
}
