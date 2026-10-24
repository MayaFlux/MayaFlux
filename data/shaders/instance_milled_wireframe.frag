#version 460

layout(location = 0) in vec3 in_color;
layout(location = 4) in vec3 in_barycentric;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 derivative = fwidth(in_barycentric);
    vec3 edge = smoothstep(vec3(0.0), derivative * 1.5, in_barycentric);
    float coverage = 1.0 - min(min(edge.x, edge.y), edge.z);
    if (coverage <= 0.0) {
        discard;
    }
    out_color = vec4(in_color, coverage);
}
