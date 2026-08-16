#ifndef VOLUME_COMMON_GLSL
#define VOLUME_COMMON_GLSL

uint volume_index(uvec3 cell, uvec3 dims)
{
    return cell.z * dims.x * dims.y + cell.y * dims.x + cell.x;
}

uint volume_index_clamped(ivec3 cell, uvec3 dims)
{
    ivec3 c = clamp(cell, ivec3(0), ivec3(dims) - 1);
    return uint(c.z) * dims.x * dims.y + uint(c.y) * dims.x + uint(c.x);
}

bool volume_in_bounds(uvec3 cell, uvec3 dims)
{
    return cell.x < dims.x && cell.y < dims.y && cell.z < dims.z;
}

vec3 volume_backtrace(uvec3 cell, vec3 velocity, vec3 cell_size, float dt, uvec3 dims)
{
    vec3 origin = vec3(cell) + 0.5 - dt * velocity / cell_size;
    return clamp(origin, vec3(0.5), vec3(dims) - 0.5);
}

void volume_trilinear_setup(vec3 p, uvec3 dims, out uint idx[8], out vec3 f)
{
    vec3 base = floor(p - 0.5);
    f = p - 0.5 - base;
    ivec3 b = ivec3(base);

    idx[0] = volume_index_clamped(b + ivec3(0, 0, 0), dims);
    idx[1] = volume_index_clamped(b + ivec3(1, 0, 0), dims);
    idx[2] = volume_index_clamped(b + ivec3(0, 1, 0), dims);
    idx[3] = volume_index_clamped(b + ivec3(1, 1, 0), dims);
    idx[4] = volume_index_clamped(b + ivec3(0, 0, 1), dims);
    idx[5] = volume_index_clamped(b + ivec3(1, 0, 1), dims);
    idx[6] = volume_index_clamped(b + ivec3(0, 1, 1), dims);
    idx[7] = volume_index_clamped(b + ivec3(1, 1, 1), dims);
}

float volume_blend8(float c[8], vec3 f)
{
    float x00 = mix(c[0], c[1], f.x);
    float x10 = mix(c[2], c[3], f.x);
    float x01 = mix(c[4], c[5], f.x);
    float x11 = mix(c[6], c[7], f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

vec4 volume_blend8(vec4 c[8], vec3 f)
{
    vec4 x00 = mix(c[0], c[1], f.x);
    vec4 x10 = mix(c[2], c[3], f.x);
    vec4 x01 = mix(c[4], c[5], f.x);
    vec4 x11 = mix(c[6], c[7], f.x);
    return mix(mix(x00, x10, f.y), mix(x01, x11, f.y), f.z);
}

#endif
