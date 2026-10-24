#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_barycentric;

void main()
{
    vec4 world_pos = transform.geometry * vec4(inPosition, 1.0);
    gl_Position = transform.projection * transform.view * world_pos;
    out_color = inColor;

    uint corner = uint(gl_VertexIndex) % 3u;
    out_barycentric = corner == 0u ? vec3(1.0, 0.0, 0.0)
        : corner == 1u ? vec3(0.0, 1.0, 0.0)
                       : vec3(0.0, 0.0, 1.0);
}
