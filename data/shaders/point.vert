#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inSize;

layout(location = 0) out vec3 out_color;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    gl_PointSize = inSize;
    out_color = inColor;
}
