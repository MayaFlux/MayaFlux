#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    float gray = texture(texSampler, fragTexCoord).r;
    outColor = vec4(gray, gray, gray, 1.0);
}
