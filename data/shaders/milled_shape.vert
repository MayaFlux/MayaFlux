#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inThickness;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 out_color;
layout(location = 1) out float out_thickness;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec3 out_world_pos;

void main()
{
    vec4 world_pos = transform.geometry * vec4(inPosition, 1.0);
    gl_Position = transform.projection * transform.view * world_pos;
    out_color = inColor;
    out_thickness = inThickness;
    out_uv = inUV;
    out_world_pos = world_pos.xyz;
}
