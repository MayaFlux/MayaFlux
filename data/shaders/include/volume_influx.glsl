#ifndef VOLUME_INFLUX_GLSL
#define VOLUME_INFLUX_GLSL

layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;

layout(set = 0, binding = 0, std430) readonly buffer FieldIn {
    float field_in[];
};
layout(set = 0, binding = 1, std430) writeonly buffer FieldOut {
    float field_out[];
};

layout(push_constant) uniform PC {
    uint width;
    uint height;
    uint depth;
    uint pad0;
    float cell_size_x;
    float cell_size_y;
    float cell_size_z;
    float elapsed;
    float center_x;
    float center_y;
    float center_z;
    float radius;
    float rate;
    float time_step;
    float falloff;
    float pad1;
    float bounds_min_x;
    float bounds_min_y;
    float bounds_min_z;
    float pad2;
} pc;

float shape(vec3 p, float t);

vec3 influx_center()
{
    return vec3(pc.center_x, pc.center_y, pc.center_z);
}

void main()
{
    uvec3 gid = gl_GlobalInvocationID;
    uvec3 dims = uvec3(pc.width, pc.height, pc.depth);

    if (!volume_in_bounds(gid, dims)) {
        return;
    }

    uint index = volume_index(gid, dims);

    vec3 cell = vec3(pc.cell_size_x, pc.cell_size_y, pc.cell_size_z);
    vec3 lo = vec3(pc.bounds_min_x, pc.bounds_min_y, pc.bounds_min_z);
    vec3 world = lo + (vec3(gid) + 0.5) * cell;

    float s = shape(world, pc.elapsed);

    field_out[index] = field_in[index] + s * pc.rate * pc.time_step;
}

#endif
