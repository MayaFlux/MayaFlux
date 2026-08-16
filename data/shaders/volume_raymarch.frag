#version 460

#extension GL_GOOGLE_include_directive : enable

#include "include/volume_common.glsl"

layout(location = 0) in vec3 frag_position;
layout(location = 1) flat in vec3 eye;

layout(set = 1, binding = 0, std430) readonly buffer VolumeField {
    float field[];
};

layout(push_constant) uniform PC {
    uint width;
    uint height;
    uint depth;
    uint max_steps;
    float bounds_min_x;
    float bounds_min_y;
    float bounds_min_z;
    float step_scale;
    float cell_size_x;
    float cell_size_y;
    float cell_size_z;
    float density_scale;
    float cool_r;
    float cool_g;
    float cool_b;
    float absorption;
    float hot_r;
    float hot_g;
    float hot_b;
    float emission;
    float threshold;
    float pad0;
    float pad1;
    float pad2;
} pc;

layout(location = 0) out vec4 out_color;

float sample_field(vec3 p, uvec3 dims)
{
    uint idx[8];
    vec3 f;
    volume_trilinear_setup(p, dims, idx, f);

    float c[8];
    for (int i = 0; i < 8; ++i) {
        c[i] = field[idx[i]];
    }

    return volume_blend8(c, f);
}

void main()
{
    uvec3 dims = uvec3(pc.width, pc.height, pc.depth);
    vec3 cell = vec3(pc.cell_size_x, pc.cell_size_y, pc.cell_size_z);
    vec3 lo = vec3(pc.bounds_min_x, pc.bounds_min_y, pc.bounds_min_z);
    vec3 hi = lo + vec3(dims) * cell;

    vec3 direction = normalize(frag_position - eye);
    vec3 inv_dir = 1.0 / direction;

    vec3 t_lo = (lo - eye) * inv_dir;
    vec3 t_hi = (hi - eye) * inv_dir;
    vec3 t_near = min(t_lo, t_hi);
    vec3 t_far = max(t_lo, t_hi);

    float enter = max(max(t_near.x, t_near.y), max(t_near.z, 0.0));
    float exit = min(min(t_far.x, t_far.y), t_far.z);

    if (exit <= enter) {
        discard;
    }

    float step_length = min(min(cell.x, cell.y), cell.z) * pc.step_scale;
    float span = exit - enter;
    uint steps = min(pc.max_steps, uint(span / step_length) + 1u);

    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    vec3 cool = vec3(pc.cool_r, pc.cool_g, pc.cool_b);
    vec3 hot = vec3(pc.hot_r, pc.hot_g, pc.hot_b);

    for (uint i = 0u; i < steps; ++i) {
        float t = enter + (float(i) + 0.5) * step_length;
        vec3 world = eye + direction * t;
        vec3 lattice = (world - lo) / cell;

        float density = sample_field(lattice, dims) * pc.density_scale;

        if (density > pc.threshold) {
            float alpha = 1.0 - exp(-density * pc.absorption * step_length);
            vec3 emitted = mix(cool, hot, clamp(density, 0.0, 1.0)) * pc.emission * density;

            accumulated += transmittance * alpha * emitted;
            transmittance *= 1.0 - alpha;

            if (transmittance < 0.01) {
                break;
            }
        }
    }

    out_color = vec4(accumulated, 1.0 - transmittance);
}
