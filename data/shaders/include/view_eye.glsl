#ifndef MF_VIEW_EYE_GLSL
#define MF_VIEW_EYE_GLSL

/**
 * Where a view looks from. Under perspective, the eye position with w one;
 * under orthographic projection, the direction toward the viewer with w zero.
 * The direction from a point at to the viewer is normalize(eye.xyz - at * eye.w).
 */
vec4 view_eye(mat4 view, mat4 projection)
{
    mat3 turn = mat3(view);
    if (projection[2][3] == 0.0) {
        return vec4(transpose(turn) * vec3(0.0, 0.0, 1.0), 0.0);
    }
    return vec4(-(transpose(turn) * view[3].xyz), 1.0);
}

#endif
