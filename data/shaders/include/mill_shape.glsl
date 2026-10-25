#ifndef MILL_SHAPE_GLSL
#define MILL_SHAPE_GLSL

#include "mill_marker.glsl"

/// @brief Edge coverage of an emulated wireframe fragment, one pixel wide.
///        Only meaningful when mill_is_wire(uv) is true.
float mill_wire_coverage(vec2 uv)
{
    float b0 = uv.x - MILL_WIRE_UV_MARKER;
    float b1 = uv.y;
    float edge = min(min(b0, b1), 1.0 - b0 - b1);
    return 1.0 - smoothstep(0.0, fwidth(edge) * 1.5, edge);
}

/// @brief A point quad's own local [0,1] UV, recovered from its marked
///        encoding. Only meaningful when mill_is_point(uv) is true.
vec2 mill_point_local_uv(vec2 uv)
{
    return fract(uv);
}

/// @brief Coverage of a milled wire edge or point disc; 1 for other shapes.
float mill_coverage(vec2 uv)
{
    if (mill_is_wire(uv)) {
        return mill_wire_coverage(uv);
    }

    if (mill_is_point(uv)) {
        return length(mill_point_local_uv(uv) - vec2(0.5)) <= 0.5 ? 1.0 : 0.0;
    }

    return 1.0;
}

/// @brief Point-quad coordinates when milled, or native point coordinates.
vec2 mill_point_coord(vec2 uv)
{
    return mill_is_point(uv) ? mill_point_local_uv(uv) : gl_PointCoord;
}

/// @brief Source UV where retained, or local coordinates for a milled point.
///        Emulated wire UV contains barycentric markers, not source UV.
vec2 mill_surface_uv(vec2 uv)
{
    return mill_is_point(uv) ? mill_point_local_uv(uv) : uv;
}

#endif
