#ifndef MILL_BILLBOARD_GLSL
#define MILL_BILLBOARD_GLSL

#include "mill_marker.glsl"

/// @brief Clip position of an instanced milled vertex, faced toward the view.
///
/// With MILL_BILLBOARD the mill leaves the source position unexpanded and
/// stores the expansion in the tangent: a point quad keeps its corner offset
/// in xy, a ribbon keeps its unit direction in xyz and signed half width in w.
/// The expansion is applied in view space after the instance transform, scaled
/// by the transform's mean scale. Without it, this is the plain projection.
vec4 mill_billboard_clip(
    mat4 view,
    mat4 projection,
    mat4 world,
    vec3 position,
    vec4 payload,
    vec2 uv,
    bool ribbon)
{
    vec4 view_pos = view * (world * vec4(position, 1.0));

    if (MILL_BILLBOARD) {
        float scale = pow(abs(determinant(mat3(world))), 1.0 / 3.0);
        if (mill_is_point(uv)) {
            view_pos.xy += payload.xy * scale;
        } else if (ribbon) {
            vec3 dir = mat3(view) * (mat3(world) * payload.xyz);
            vec3 side = cross(dir, -view_pos.xyz);
            float side_length = length(side);
            if (side_length > 1e-8) {
                view_pos.xyz += side / side_length * payload.w * scale;
            }
        }
    }

    return projection * view_pos;
}

#endif
