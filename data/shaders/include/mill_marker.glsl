#ifndef MILL_MARKER_GLSL
#define MILL_MARKER_GLSL

#include "mill_config.glsl"

/// @brief UV threshold marking a milled point-quad corner.
///
/// A milled point corner uses [MARKER, MARKER + 1] instead of [0,1].
/// Ribbons and passthrough triangles retain their UV. The specialization
/// guard prevents native UV from being mistaken for a shape marker.
const float MILL_POINT_UV_MARKER = 10.0;

/// @brief UV threshold marking an emulated wireframe triangle corner.
///
/// A wire corner carries two of its barycentric weights in the UV slot as
/// (MARKER + b0, b1); the third is 1 - b0 - b1. It replaces the source UV.
const float MILL_WIRE_UV_MARKER = 20.0;

/// @brief Whether a milled fragment's UV marks it as a point-quad corner.
bool mill_is_point(vec2 uv)
{
    return MILL_ACTIVE && uv.x >= MILL_POINT_UV_MARKER && uv.x < MILL_WIRE_UV_MARKER;
}

/// @brief Whether a milled fragment's UV marks it as part of an emulated
///        wireframe triangle.
bool mill_is_wire(vec2 uv)
{
    return MILL_ACTIVE && uv.x >= MILL_WIRE_UV_MARKER;
}

#endif
