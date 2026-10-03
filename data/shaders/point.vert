#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_position;

void main()
{
    vec4 world_pos = transform.geometry * vec4(inPosition, 1.0);
    gl_Position = transform.projection * transform.view * world_pos;
    gl_PointSize = inSize;
    out_color = inColor;
    out_position = world_pos.xyz;
}
