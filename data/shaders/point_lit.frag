#version 450 core

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec3 in_world_pos;

layout(set = 1, binding = 0) uniform Influence {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
};

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 coord = gl_PointCoord - vec2(0.5);
    if (length(coord) > 0.5) {
        discard;
    }

    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = in_color + (color * intensity * attenuation);
    outColor = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
