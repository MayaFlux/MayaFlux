#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inThickness;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 out_color;
layout(location = 1) out float out_thickness;

void main()
{
    gl_Position = vec4(inPosition, 1.0);
    out_color = inColor;
    out_thickness = inThickness;
}
