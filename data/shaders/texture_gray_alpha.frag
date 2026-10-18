#version 460

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    vec2 grayAlpha = texture(texSampler, fragTexCoord).rg;
    outColor = vec4(grayAlpha.r, grayAlpha.r, grayAlpha.r, grayAlpha.g);
}
