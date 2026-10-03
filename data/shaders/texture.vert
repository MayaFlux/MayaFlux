#version 460

layout(set = 0, binding = 0) uniform RenderTransformBlock {
    mat4 view;
    mat4 projection;
    mat4 geometry;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 out_world_pos;

void main()
{
    vec4 world_pos = transform.geometry * vec4(inPosition, 1.0);
    gl_Position = transform.projection * transform.view * world_pos;
    fragTexCoord = inTexCoord;
    out_world_pos = world_pos.xyz;
}
