#version 450 core

layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(in_color, 1.0);

    vec2 coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5) {
        discard;
    }
}
