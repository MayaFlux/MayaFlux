#version 450 core

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 outColor;

void main()
{
    float edge_softness = 0.1;
    float dist_from_center = abs(in_uv.y - 0.5) * 2.0;
    float alpha = 1.0 - smoothstep(1.0 - edge_softness, 1.0, dist_from_center);

    outColor = vec4(in_color, alpha);
}
