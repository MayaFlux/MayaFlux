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

    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;

    vec2 dir = normalize(ndc1 - ndc0);
    vec2 normal = vec2(-dir.y, dir.x);

    float t0 = in_thickness[0] * 0.005;
    float t1 = in_thickness[1] * 0.005;

    gl_Position = vec4((ndc0 + normal * t0) * p0.w, p0.z, p0.w);
    out_color = in_color[0];
    out_uv = vec2(0.0, 0.0);
    EmitVertex();

    gl_Position = vec4((ndc0 - normal * t0) * p0.w, p0.z, p0.w);
    out_color = in_color[0];
    out_uv = vec2(0.0, 1.0);
    EmitVertex();

    gl_Position = vec4((ndc1 + normal * t1) * p1.w, p1.z, p1.w);
    out_color = in_color[1];
    out_uv = vec2(1.0, 0.0);
    EmitVertex();

    gl_Position = vec4((ndc1 - normal * t1) * p1.w, p1.z, p1.w);
    out_color = in_color[1];
    out_uv = vec2(1.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
