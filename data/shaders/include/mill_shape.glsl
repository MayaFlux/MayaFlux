#ifndef MILL_SHAPE_GLSL
#define MILL_SHAPE_GLSL

/// @brief UV threshold marking a milled point-quad corner.
///
/// A triangulated draw shares one fragment shader across every span shape,
/// so nothing else says which one a fragment came from. Rather than cost
/// the shared vertex layout a shape field, a point corner's UV lands in
/// [MARKER, MARKER + 1] instead of [0,1]; a ribbon's stays in [0,1] and a
/// passthrough triangle keeps its source UV. Any fragment shader can
/// recover shape identity from raw UV alone.
const float MILL_POINT_UV_MARKER = 10.0;

/// @brief UV threshold marking an emulated wireframe triangle corner.
///
/// A wire corner carries two of its barycentric weights in the UV slot as
/// (MARKER + b0, b1); the third is 1 - b0 - b1. It replaces the source UV.
const float MILL_WIRE_UV_MARKER = 20.0;

/// @brief Whether a milled fragment's UV marks it as a point-quad corner.
bool mill_is_point(vec2 uv)
{
    return uv.x >= MILL_POINT_UV_MARKER && uv.x < MILL_WIRE_UV_MARKER;
}

/// @brief Whether a milled fragment's UV marks it as part of an emulated
///        wireframe triangle.
bool mill_is_wire(vec2 uv)
{
    return uv.x >= MILL_WIRE_UV_MARKER;
}

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

#endif
