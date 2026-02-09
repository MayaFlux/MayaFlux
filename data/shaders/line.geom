#version 450 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec3 in_color[];
layout(location = 1) in float in_thickness[];

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec2 out_uv;

void main()
{
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    vec2 dir = normalize(p1.xy - p0.xy);
    vec2 normal = vec2(-dir.y, dir.x);

    float thickness0 = in_thickness[0] * 0.005;
    float thickness1 = in_thickness[1] * 0.005;

    gl_Position = vec4(p0.xy + normal * thickness0, p0.z, 1.0);
    out_color = in_color[0];
    out_uv = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = vec4(p0.xy - normal * thickness0, p0.z, 1.0);
    out_color = in_color[0];
    out_uv = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p1.xy + normal * thickness1, p1.z, 1.0);
    out_color = in_color[1];
    out_uv = vec2(1.0, 0.0);
    EmitVertex();

    gl_Position = vec4(p1.xy - normal * thickness1, p1.z, 1.0);
    out_color = in_color[1];
    out_uv = vec2(1.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
