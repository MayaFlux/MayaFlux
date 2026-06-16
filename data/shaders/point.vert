#version 450 core

layout(set = 0, binding = 0) uniform ViewTransformBlock {
    mat4 view;
    mat4 projection;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_position;

void main()
{
    gl_Position = transform.projection * transform.view * vec4(inPosition, 1.0);
    gl_PointSize = inSize;
    out_color = inColor;
    out_position = inPosition;
}
