#pragma once

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// Filled shapes — Kakshya::Vertex, TRIANGLE_LIST topology
//
// All coordinates are 2D NDC (z = 0). These produce vertex arrays suitable
// for direct use with write_verts / FormaBuffer::submit / create_buffer.
// No index buffer is required; triangle fan decomposition is inlined.
// =============================================================================

/**
 * @brief Filled circle as a TRIANGLE_LIST triangle fan.
 *
 * Decomposes into (segments) triangles sharing a common center vertex.
 * Minimum @p segments is 3; values below are clamped.
 *
 * @param center   Circle center in NDC.
 * @param radius   Radius in NDC units.
 * @param segments Triangle count around the circumference.
 * @param color    Uniform fill color.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::Vertex> filled_circle(
    glm::vec2 center,
    float radius,
    uint32_t segments,
    glm::vec3 color = glm::vec3(1.F));

/**
 * @brief Filled annulus (ring) as a TRIANGLE_LIST quad strip.
 *
 * Produces two triangles per angular segment between @p inner_r and
 * @p outer_r. Minimum @p segments is 3.
 *
 * @param center   Ring center in NDC.
 * @param inner_r  Inner radius in NDC units.
 * @param outer_r  Outer radius in NDC units.
 * @param segments Quad count around the circumference.
 * @param color    Uniform fill color.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::Vertex> filled_ring(
    glm::vec2 center,
    float inner_r,
    float outer_r,
    uint32_t segments,
    glm::vec3 color = glm::vec3(1.F));

/**
 * @brief Filled regular n-gon as a TRIANGLE_LIST triangle fan.
 *
 * Vertices are placed on a circle of @p radius, starting at
 * @p rotation_rad from +X, CCW. Minimum @p sides is 3.
 *
 * @param center       Polygon center in NDC.
 * @param radius       Circumradius in NDC units.
 * @param sides        Number of sides. Clamped to minimum 3.
 * @param rotation_rad Starting angle in radians from +X axis.
 * @param color        Uniform fill color.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::Vertex> filled_polygon(
    glm::vec2 center,
    float radius,
    uint32_t sides,
    float rotation_rad = 0.F,
    glm::vec3 color = glm::vec3(1.F));

/**
 * @brief Filled rect with per-corner colors, TRIANGLE_STRIP (4 vertices).
 *
 * Vertex order matches filled_rect: BL, BL-top, BR, BR-top (TRIANGLE_STRIP).
 * Drop-in replacement for filled_rect when a gradient fill is needed.
 *
 * @param region   NDC axis-aligned bounds.
 * @param color_bl Bottom-left corner color.
 * @param color_br Bottom-right corner color.
 * @param color_tl Top-left corner color.
 * @param color_tr Top-right corner color.
 */
[[nodiscard]] MAYAFLUX_API std::array<Kakshya::Vertex, 4> filled_rect_gradient(
    AABB2D region,
    glm::vec3 color_bl,
    glm::vec3 color_br,
    glm::vec3 color_tl,
    glm::vec3 color_tr);

/**
 * @brief Filled rounded rectangle as a TRIANGLE_LIST mesh.
 *
 * Composed of a center rect, four edge rects, and four corner fans.
 * @p corner_radius is clamped to half the shorter axis to prevent overlap.
 * Minimum @p corner_segments is 1.
 *
 * @param region          NDC axis-aligned bounds.
 * @param corner_radius   Radius of each rounded corner in NDC units.
 * @param corner_segments Triangle count per quarter-circle corner arc.
 * @param color           Uniform fill color.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::Vertex> filled_rounded_rect(
    AABB2D region,
    float corner_radius,
    uint32_t corner_segments,
    glm::vec3 color = glm::vec3(1.F));

/**
 * @brief Filled circular arc sector (pie slice) as a TRIANGLE_LIST fan.
 *
 * @p angle_start and @p angle_end are in radians, measured from +X CCW.
 * Minimum @p segments is 1.
 *
 * @param center      Arc center in NDC.
 * @param radius      Radius in NDC units.
 * @param angle_start Start angle in radians.
 * @param angle_end   End angle in radians.
 * @param segments    Triangle count over the arc span.
 * @param color       Uniform fill color.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::Vertex> filled_arc(
    glm::vec2 center,
    float radius,
    float angle_start,
    float angle_end,
    uint32_t segments,
    glm::vec3 color = glm::vec3(1.F));

/**
 * @brief Sample a circular arc as ordered NDC positions.
 *
 * Produces @p segments + 1 points along the arc from @p angle_start to
 * @p angle_end, suitable as path input to stroke_slider or any other
 * function consuming a @c span<const glm::vec2> path.
 *
 * @param center      Arc center in NDC.
 * @param radius_x    Horizontal radius in NDC units.
 * @param radius_y    Vertical radius in NDC units (use == radius_x for a circle).
 * @param angle_start Start angle in radians from +X, CCW.
 * @param angle_end   End angle in radians from +X, CCW.
 * @param segments    Number of segments; produces segments+1 points.
 */
[[nodiscard]] MAYAFLUX_API std::vector<glm::vec2> arc_path(
    glm::vec2 center,
    float radius_x,
    float radius_y,
    float angle_start,
    float angle_end,
    uint32_t segments);

// =============================================================================
// Outlines — Kakshya::LineVertex, LINE_LIST topology
//
// Each function produces adjacent vertex pairs for LINE_LIST draw.
// LINE_STRIP callers can use every other vertex as the strip is equivalent
// for these closed/open forms, but LINE_LIST is the canonical output to
// match the existing Forma pipeline (radial, stroke_slider precedent).
// =============================================================================

/**
 * @brief Circle outline as a LINE_LIST (closed loop, 2 * segments vertices).
 *
 * Each segment is one line: [ring[i], ring[(i+1) % segments]].
 * Minimum @p segments is 3.
 *
 * @param center    Circle center in NDC.
 * @param radius    Radius in NDC units.
 * @param segments  Edge count around the circumference.
 * @param color     Uniform line color.
 * @param thickness Line thickness (maps to LineVertex::thickness).
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> circle_outline(
    glm::vec2 center,
    float radius,
    uint32_t segments,
    glm::vec3 color = glm::vec3(1.F),
    float thickness = 1.F);

/**
 * @brief Arc outline as a LINE_LIST (open curve, 2 * segments vertices).
 *
 * @p angle_start and @p angle_end are in radians from +X, CCW.
 * Minimum @p segments is 1.
 *
 * @param center      Arc center in NDC.
 * @param radius      Radius in NDC units.
 * @param angle_start Start angle in radians.
 * @param angle_end   End angle in radians.
 * @param segments    Edge count over the arc span.
 * @param color       Uniform line color.
 * @param thickness   Line thickness.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> arc_outline(
    glm::vec2 center,
    float radius,
    float angle_start,
    float angle_end,
    uint32_t segments,
    glm::vec3 color = glm::vec3(1.F),
    float thickness = 1.F);

/**
 * @brief Rectangle outline as a LINE_LIST (4 edges, 8 vertices).
 *
 * @param region    NDC axis-aligned bounds.
 * @param color     Uniform line color.
 * @param thickness Line thickness.
 */
[[nodiscard]] MAYAFLUX_API std::array<Kakshya::LineVertex, 8> rect_outline(
    AABB2D region,
    glm::vec3 color = glm::vec3(1.F),
    float thickness = 1.F);

/**
 * @brief Polyline as a LINE_LIST (open path, 2 * (pts.size() - 1) vertices).
 *
 * Converts a span of NDC vec2 positions into adjacent LineVertex pairs.
 * Suitable for waveform traces, cable routes, and arbitrary open curves.
 *
 * @param pts       Ordered path vertices in NDC.
 * @param color     Uniform line color.
 * @param thickness Line thickness.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> polyline(
    std::span<const glm::vec2> pts,
    glm::vec3 color = glm::vec3(1.F),
    float thickness = 1.F);

/**
 * @brief Polyline with per-vertex colors as a LINE_LIST.
 *
 * @p pts and @p colors must have equal length. Each line segment blends
 * from the color at its start vertex to the color at its end vertex.
 *
 * @param pts    Ordered path vertices in NDC.
 * @param colors Per-vertex colors. Must match @p pts in size.
 * @param thickness Line thickness applied uniformly.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> polyline_colored(
    std::span<const glm::vec2> pts,
    std::span<const glm::vec3> colors,
    float thickness = 1.F);

/**
 * @brief Regular n-gon outline as a LINE_LIST (closed loop, 2 * sides vertices).
 *
 * @param center       Polygon center in NDC.
 * @param radius       Circumradius in NDC units.
 * @param sides        Side count. Clamped to minimum 3.
 * @param rotation_rad Starting angle in radians from +X axis.
 * @param color        Uniform line color.
 * @param thickness    Line thickness.
 */
[[nodiscard]] MAYAFLUX_API std::vector<Kakshya::LineVertex> polygon_outline(
    glm::vec2 center,
    float radius,
    uint32_t sides,
    float rotation_rad = 0.F,
    glm::vec3 color = glm::vec3(1.F),
    float thickness = 1.F);

} // namespace MayaFlux::Kinesis
