#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_world_pos;

layout(set = 0, binding = 1) uniform sampler2D diffuseTex;

layout(set = 1, binding = 1) uniform Influence {
    vec3 position;
    float intensity;
    vec3 color;
    float radius;
    float size;
};

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 tex = texture(diffuseTex, in_uv);

    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = tex.rgb + (color * intensity * attenuation);
    out_color = vec4(clamp(lit, 0.0, 1.0), tex.a);
}
