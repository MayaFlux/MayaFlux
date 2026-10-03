#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in float in_weight;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 in_normal;
layout(location = 5) in vec3 in_tangent;

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} pc;

layout(location = 0) out vec3 out_position;
layout(location = 1) flat out vec3 out_eye;

void main()
{
    gl_Position = pc.projection * pc.view * pc.geometry * vec4(in_position, 1.0);
    out_position = in_position;
    vec4 local_eye = inverse(pc.view * pc.geometry) * vec4(0.0, 0.0, 0.0, 1.0);
    out_eye = local_eye.xyz / local_eye.w;
}
