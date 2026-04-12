#version 450

layout(set = 0, binding = 0) uniform ViewTransformBlock {
    mat4 view;
    mat4 projection;
} transform;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 out_world_pos;

void main()
{
    gl_Position = transform.projection * transform.view * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    out_world_pos = inPosition;
}
