#ifndef SDF_FIELD_COMMON_GLSL
#define SDF_FIELD_COMMON_GLSL

#include "sdf_primitives.glsl"
#include "noise.glsl"
#include "domain_warp.glsl"

layout(local_size_x = 64) in;

layout(set = 0, binding = 0) buffer GridBuffer {
    float grid[];
};

layout(push_constant) uniform PC {
    vec3 bounds_min;
    float time;
    vec3 step;
    uint res_x;
    uint res_y;
    uint res_z;
    uint _pad[2];
} pc;

/// @brief Compute the grid corner position for the current invocation.
/// @param idx  gl_GlobalInvocationID.x
/// @return     World-space position of the corner, or vec3(0) if out of range.
bool sdf_get_corner(uint idx, out vec3 p)
{
    uint nx = pc.res_x + 1u;
    uint ny = pc.res_y + 1u;
    uint total = nx * ny * (pc.res_z + 1u);
    if (idx >= total)
        return false;

    uint gix = idx % nx;
    uint giy = (idx / nx) % ny;
    uint giz = idx / (nx * ny);

    p = pc.bounds_min + vec3(
                float(gix) * pc.step.x,
                float(giy) * pc.step.y,
                float(giz) * pc.step.z);
    return true;
}

#endif // SDF_FIELD_COMMON_GLSL
