#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;

layout(push_constant) uniform PushConstants {
    mat4 view;
    mat4 projection;
} pc;

layout(location = 0) out vec3 out_color;

void main()
{
    gl_Position = pc.projection * pc.view * vec4(in_position, 1.0);
    out_color = in_color;
}
