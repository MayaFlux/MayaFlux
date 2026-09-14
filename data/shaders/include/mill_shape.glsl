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

/// @brief Whether a milled fragment's UV marks it as a point-quad corner.
bool mill_is_point(vec2 uv)
{
    return uv.x >= MILL_POINT_UV_MARKER;
}

/// @brief A point quad's own local [0,1] UV, recovered from its marked
///        encoding. Only meaningful when mill_is_point(uv) is true.
vec2 mill_point_local_uv(vec2 uv)
{
    return fract(uv);
}

#endif
