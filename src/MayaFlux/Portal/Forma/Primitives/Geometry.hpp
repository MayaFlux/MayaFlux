#pragma once

#include "Mapped.hpp"

namespace MayaFlux::Portal::Forma {
class Context;
}

namespace MayaFlux::Portal::Forma::Geometry {

/**
 * @file Geometry.hpp
 * @brief Illustrative geometry functions for common Mapped use cases.
 *
 * These are starting points for reading and understanding the GeometryFn
 * contract, not the primary or idiomatic way to use Mapped in MayaFlux.
 *
 * The idiomatic use is a custom geometry function that writes from whatever
 * data source makes sense: mouse pixel coordinates written directly as buffer
 * data, microphone energy mapped to a spatial form, a node output driving
 * vertex positions, a tendency field evaluated at runtime. The function
 * receives a value and an output byte buffer — what it does with those is
 * unconstrained.
 *
 * The helpers below demonstrate the pattern concretely. They are not
 * privileged and carry no special status in the framework.
 */

/**
 * @brief Write a vertex array into a GeometryFn output buffer.
 * @param out   Output buffer. Resized to fit @p verts exactly.
 * @param verts Vertices to copy.
 */
template <typename V>
void write_verts(std::vector<uint8_t>& out, const std::vector<V>& verts)
{
    out.resize(verts.size() * sizeof(V));
    std::memcpy(out.data(), verts.data(), out.size());
}

/**
 * @brief Write a contiguous range of trivially-copyable vertices into a GeometryFn output buffer.
 */
template <typename V>
    requires std::ranges::contiguous_range<V>
    && std::is_trivially_copyable_v<std::ranges::range_value_t<V>>
void write_verts(std::vector<uint8_t>& out, const V& verts)
{
    const size_t n = std::ranges::size(verts) * sizeof(std::ranges::range_value_t<V>);
    out.resize(n);
    std::memcpy(out.data(), std::ranges::data(verts), n);
}

template <typename V>
    requires std::is_trivially_copyable_v<V>
    && (!std::ranges::range<V>)
void write_verts(std::vector<uint8_t>& out, const V& v)
{
    out.resize(sizeof(v));
    std::memcpy(out.data(), &v, sizeof(v));
}

// =============================================================================
// Horizontal fader
//
// Value in [0, 1] moves a handle quad along a track quad.
// Two quads: track (static) and handle (value-driven).
// Both written as TRIANGLE_STRIP pairs into a single buffer.
//
// Track: full extent of bounds.
// Handle: small square at x = bounds.min.x + value * bounds.width().
//
// Hit region follows the handle, updated on every sync().
// =============================================================================

/**
 * @brief Geometry function for a horizontal fader in NDC space.
 * @param bounds      Full extent of the fader in NDC.
 * @param handle_w    Handle width in NDC units.
 * @param track_color Track quad color.
 * @param handle_color Handle quad color.
 */
[[nodiscard]] GeometryFn<float> horizontal_fader(
    Kinesis::AABB2D bounds,
    float handle_w,
    glm::vec3 track_color = glm::vec3(0.3F),
    glm::vec3 handle_color = glm::vec3(0.9F));

// =============================================================================
// Vertical fader
//
// Symmetric counterpart to horizontal_fader. Value in [0, 1] moves a handle
// quad upward along a track quad.
//
// Track: thin vertical band through the center of bounds.
// Handle: small square at y = bounds.min.y + value * (bounds.height() - handle_h).
//
// Hit region follows the handle, updated on every sync().
// Topology: TRIANGLE_STRIP (two quad pairs via to_mesh_vertices).
// =============================================================================

/**
 * @brief Geometry function for a vertical fader in NDC space.
 * @param bounds       Full extent of the fader in NDC.
 * @param handle_h     Handle height in NDC units.
 * @param track_color  Track quad color.
 * @param handle_color Handle quad color.
 */
[[nodiscard]] GeometryFn<float> vertical_fader(
    Kinesis::AABB2D bounds,
    float handle_h,
    glm::vec3 track_color = glm::vec3(0.3F),
    glm::vec3 handle_color = glm::vec3(0.9F));

// =============================================================================
// Radial / arc
//
// Value in [0, 1] sweeps an indicator line around a center point.
// angle_start and angle_end in radians, measured from +X, CCW.
// Produces a LINE_LIST of two vertices: center to indicator tip.
// =============================================================================

/**
 * @brief Geometry function for a radial indicator in NDC space.
 * @param center      Arc center in NDC.
 * @param radius      Arc radius in NDC units.
 * @param angle_start Start angle in radians (value = 0).
 * @param angle_end   End angle in radians (value = 1).
 * @param color       Line color.
 */
[[nodiscard]] GeometryFn<float> radial(
    glm::vec2 center,
    float radius,
    float angle_start,
    float angle_end,
    glm::vec3 color = glm::vec3(0.9F));

// =============================================================================
// Point
//
// Value is glm::vec2 in NDC space. Produces a single POINT_LIST vertex.
// Hit region is a circle centered on the point.
// Use as a cursor follower, node handle, or any positioned point primitive.
// =============================================================================

/**
 * @brief Geometry function for a positioned point in NDC space.
 *
 * Value type is glm::vec2 (NDC position). Renders a single PointVertex
 * and sets a circular hit region centered on the position. Suitable as
 * a cursor follower, node handle, or any draggable point primitive.
 *
 * @param color      Point color.
 * @param size       Point size in pixels.
 * @param hit_radius Hit region radius in NDC units.
 */
[[nodiscard]] GeometryFn<glm::vec2> point(
    glm::vec3 color = glm::vec3(1.0F),
    float size = 10.0F,
    float hit_radius = 0.04F);

// =============================================================================
// 2D position picker
//
// Value is glm::vec2 in [0,1]x[0,1], mapped to a point inside bounds.
// Produces a single POINT_LIST vertex at the mapped position.
// =============================================================================

/**
 * @brief Geometry function for a 2D position picker in NDC space.
 * @param bounds Full extent of the pick area in NDC.
 * @param color  Point color.
 * @param size   Point size in pixels.
 */
[[nodiscard]] GeometryFn<glm::vec2> position_picker(
    Kinesis::AABB2D bounds,
    glm::vec3 color = glm::vec3(0.9F),
    float size = 8.0F);

// =============================================================================
// Stroke slider
//
// Value in [0, 1] positions a handle point along a user-supplied polyline.
// Renders two layers: the full path as a LINE_LIST in track_color, a
// highlighted prefix segment from path start to the handle position in
// fill_color, and a PointVertex handle on a caller-supplied secondary buffer.
//
// The secondary buffer must be a POINT_LIST FormaBuffer registered with the
// same BufferManager and window before this function is called. The caller
// adds it as a separate Element and relates it to the path element via
// Layer::relate_to so removal and visibility cascade.
//
// Hit region: stroke_bounds over the full path at half_thickness.
// bounds_hint: tight AABB enclosing all path points.
//
// Topology for the path buffer: LINE_LIST.
// Topology for the handle buffer: POINT_LIST.
// =============================================================================

/**
 * @brief Geometry function for a value scrubber along an arbitrary polyline.
 *
 * Value in [0, 1] maps to arc-length position along @p path. The full path
 * is rendered as a LINE_LIST in @p track_color; the prefix from the path
 * start to the handle position is rendered in @p fill_color. The handle
 * itself is a PointVertex submitted to @p handle_buf each sync.
 *
 * @param path           Ordered polyline vertices in NDC. Copied into closure.
 * @param handle_buf     POINT_LIST FormaBuffer for the handle point.
 *                       Must be registered and have setup_rendering called.
 * @param half_thickness Hit region half-thickness in NDC units.
 * @param track_color    Color of the full path.
 * @param fill_color     Color of the prefix segment up to the handle.
 * @param handle_color   Color of the handle point.
 * @param handle_size    Handle point size in pixels.
 */
[[nodiscard]] GeometryFn<float> stroke_slider(
    std::span<const glm::vec2> path,
    std::shared_ptr<Buffers::FormaBuffer> handle_buf,
    float half_thickness = 0.02F,
    glm::vec3 track_color = glm::vec3(0.3F),
    glm::vec3 fill_color = glm::vec3(0.2F, 0.6F, 1.0F),
    glm::vec3 handle_color = glm::vec3(0.95F),
    float handle_size = 10.0F);

// =============================================================================
// Toggle
//
// Value type: bool. Renders a filled rect in one of two colors.
// The geometry function is purely visual — it carries no interaction.
// The caller wires on_press to flip state->value:
//
//   surface.ctx().on_press(el.element.id, IO::MouseButtons::Left,
//       [state = el.state](uint32_t, glm::vec2) {
//           state->write(!state->value);
//       });
//
// Topology: TRIANGLE_STRIP (4 MeshVertex via to_mesh_vertices).
// =============================================================================

/**
 * @brief Geometry function for a boolean toggle in NDC space.
 *
 * Renders @p region as a filled rect in @p color_off or @p color_on based
 * on the current bool value. Interaction is the caller's responsibility.
 *
 * @param region    NDC bounds of the toggle.
 * @param color_off Fill color when false.
 * @param color_on  Fill color when true.
 */
[[nodiscard]] GeometryFn<bool> toggle(
    Kinesis::AABB2D region,
    glm::vec3 color_off = glm::vec3(0.25F),
    glm::vec3 color_on = glm::vec3(0.2F, 0.7F, 0.4F));

// =============================================================================
// Level meter
//
// Value type: float in [0, 1]. Renders a filled bar from the origin edge of
// bounds proportional to the value, with the remainder as a second color.
// No handle, no hit region. Suitable for audio level, progress, any scalar readout.
//
// Orientation:
//   horizontal = true  - bar grows left to right
//   horizontal = false - bar grows bottom to top
//
// Topology: TRIANGLE_STRIP (two quad pairs via to_mesh_vertices).
// =============================================================================

/**
 * @brief Geometry function for a level meter in NDC space.
 *
 * No interaction. Drive from a node via bridge().at(el.state).bind(node).
 *
 * @param bounds      Full extent of the meter in NDC.
 * @param horizontal  True for left-to-right fill, false for bottom-to-top.
 * @param fill_color  Color of the active (filled) portion.
 * @param track_color Color of the inactive remainder.
 */
[[nodiscard]] GeometryFn<float> level_meter(
    Kinesis::AABB2D bounds,
    bool horizontal = true,
    glm::vec3 fill_color = glm::vec3(0.2F, 0.7F, 0.3F),
    glm::vec3 track_color = glm::vec3(0.15F));

// =============================================================================
// Crosshair
//
// Value type: glm::vec2 (NDC position). Renders two LINE_LIST segments —
// horizontal and vertical — crossing at the value position.
// Hit region: circle centered on the position.
//
// Pairs naturally with position_picker sharing the same MappedState<glm::vec2>.
// Topology: LINE_LIST (4 LineVertex).
// =============================================================================

/**
 * @brief Geometry function for a crosshair indicator in NDC space.
 *
 * @param arm_len    Half-length of each arm in NDC units.
 * @param color      Line color.
 * @param thickness  Line thickness (maps to LineVertex::thickness).
 * @param hit_radius Hit region radius in NDC units.
 *
 * @note Pass @c PrimitiveTopology::LINE_LIST explicitly to create_element —
 *       the default TRIANGLE_STRIP will misinterpret the 4 vertices.
 */
[[nodiscard]] GeometryFn<glm::vec2> crosshair(
    float arm_len = 0.04F,
    glm::vec3 color = glm::vec3(0.9F),
    float thickness = 1.F,
    float hit_radius = 0.05F);

// =============================================================================
// Drawable canvas
//
// value is vector<float> of N samples in [0, 1] mapped to the Y axis.
// X positions are distributed evenly across bounds.
// Renders as LINE_LIST: N-1 segments connecting adjacent samples.
//
// on_drag callback pattern:
//   ctx->on_drag(el.element.id, IO::MouseButtons::Left,
//       [&el, bounds](uint32_t, glm::vec2 ndc) {
//           auto& v = el.state->value;
//           const float t = (ndc.x - bounds.min.x) / bounds.width();
//           const size_t i = static_cast<size_t>(
//               std::clamp(t, 0.F, 1.F) * (v.size() - 1));
//           const float a = (ndc.y - bounds.min.y) / bounds.height();
//           v[i] = std::clamp(a, 0.F, 1.F);
//           ++el.state->version;
//       });
//
// For sparse-sample prevention at high drag speed, interpolate between the
// previous and current index before calling state->write().
//
// Topology: LINE_LIST.
// =============================================================================

/**
 * @brief Geometry function for a drawable curve canvas in NDC space.
 *
 * Renders the sample vector as a LINE_LIST polyline. Each adjacent sample
 * pair becomes one segment. The hit region covers the full canvas bounds.
 *
 * @param bounds      Canvas extent in NDC.
 * @param color       Line color.
 * @param thickness   LineVertex thickness value.
 */
[[nodiscard]] GeometryFn<std::vector<float>> drawable_canvas(
    Kinesis::AABB2D bounds,
    glm::vec3 color = glm::vec3(0.8F),
    float thickness = 1.5F);

/**
 * @brief Wire drag interaction for a drawable canvas element.
 *
 * Registers a left-button drag callback on @p ctx that maps NDC cursor
 * position to a sample index and amplitude, writes into @p state, and
 * increments the version. Linear interpolation fills the range between the
 * previously touched index and the current one, preventing sparse samples
 * under fast drag.
 *
 * @param ctx     Context owning the element.
 * @param id      Element id from the Mapped.
 * @param state   MappedState<vector<float>> to write into.
 * @param bounds  Canvas NDC bounds — must match those passed to drawable_canvas().
 */
void wire_canvas_drag(
    Context& ctx,
    uint32_t id,
    std::shared_ptr<MappedState<std::vector<float>>> state,
    Kinesis::AABB2D bounds);

} // namespace MayaFlux::Portal::Forma::Geometry
