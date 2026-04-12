#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 in_world_pos;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

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
    vec4 tex = texture(texSampler, fragTexCoord);

    float dist = length(position - in_world_pos);
    float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
    attenuation *= attenuation;

    vec3 lit = tex.rgb + (color * intensity * attenuation);
    outColor = vec4(clamp(lit, 0.0, 1.0), tex.a);
}
