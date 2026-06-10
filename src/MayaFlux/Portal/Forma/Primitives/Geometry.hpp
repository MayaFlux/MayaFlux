#pragma once

#include "Mapped.hpp"

#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

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
[[nodiscard]] inline GeometryFn<float> horizontal_fader(
    Kinesis::AABB2D bounds,
    float handle_w,
    glm::vec3 track_color = glm::vec3(0.3F),
    glm::vec3 handle_color = glm::vec3(0.9F))
{
    return [bounds, handle_w, track_color, handle_color](
               float v, std::vector<uint8_t>& out, Element& el) {
        float x = bounds.min.x + v * (bounds.width() - handle_w);
        float yt = bounds.min.y + bounds.height() * 0.35F;
        float yb = bounds.min.y + bounds.height() * 0.65F;

        Kinesis::AABB2D track { .min = glm::vec2(bounds.min.x, yt), .max = glm::vec2(bounds.max.x, yb) };
        Kinesis::AABB2D handle { .min = glm::vec2(x, bounds.min.y), .max = glm::vec2(x + handle_w, bounds.max.y) };

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(track, track_color));
        auto herts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(handle, handle_color));
        verts.insert(verts.end(), herts.begin(), herts.end());

        write_verts(out, verts);

        el.bounds_hint = handle;
        el.contains = {};
    };
}

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
[[nodiscard]] inline GeometryFn<float> radial(
    glm::vec2 center,
    float radius,
    float angle_start,
    float angle_end,
    glm::vec3 color = glm::vec3(0.9F))
{
    return [center, radius, angle_start, angle_end, color](
               float v, std::vector<uint8_t>& out, Element& el) {
        float angle = angle_start + v * (angle_end - angle_start);
        glm::vec2 tip = center + radius * glm::vec2(std::cos(angle), std::sin(angle));

        using V = Kakshya::LineVertex;
        std::vector<V> verts = {
            { .position = { center.x, center.y, 0 }, .color = color },
            { .position = { tip.x, tip.y, 0 }, .color = color },
        };

        write_verts(out, verts);

        el.bounds_hint = Kinesis::AABB2D::from_ndc(center, glm::vec2(radius));
        el.contains = Kinesis::circular_bounds(center, radius);
    };
}

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
[[nodiscard]] inline GeometryFn<glm::vec2> point(
    glm::vec3 color = glm::vec3(1.0F),
    float size = 10.0F,
    float hit_radius = 0.04F)
{
    return [color, size, hit_radius](glm::vec2 pos, std::vector<uint8_t>& out, Element& el) {
        write_verts(out, Kakshya::PointVertex {
                             .position = { pos.x, pos.y, 0.0F },
                             .color = color,
                             .size = size,
                         });
        el.bounds_hint = Kinesis::AABB2D::from_ndc(pos, glm::vec2(hit_radius));
        el.contains = Kinesis::circular_bounds(pos, hit_radius);
    };
}

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
[[nodiscard]] inline GeometryFn<glm::vec2> position_picker(
    Kinesis::AABB2D bounds,
    glm::vec3 color = glm::vec3(0.9F),
    float size = 8.0F)
{
    return [bounds, color, size](
               glm::vec2 v, std::vector<uint8_t>& out, Element& el) {
        float x = bounds.min.x + v.x * bounds.width();
        float y = bounds.min.y + v.y * bounds.height();

        using V = Kakshya::PointVertex;
        std::vector<V> verts = {
            { .position = { x, y, 0 }, .color = color, .size = size },
        };

        write_verts(out, verts);

        el.bounds_hint = bounds;
        el.contains = Kinesis::polygon_bounds(std::span<const glm::vec2> {
            std::array<glm::vec2, 4> {
                bounds.min,
                glm::vec2(bounds.max.x, bounds.min.y),
                bounds.max,
                glm::vec2(bounds.min.x, bounds.max.y) } });
    };
}

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
[[nodiscard]] inline GeometryFn<float> stroke_slider(
    std::span<const glm::vec2> path,
    std::shared_ptr<Buffers::FormaBuffer> handle_buf,
    float half_thickness = 0.02F,
    glm::vec3 track_color = glm::vec3(0.3F),
    glm::vec3 fill_color = glm::vec3(0.2F, 0.6F, 1.0F),
    glm::vec3 handle_color = glm::vec3(0.95F),
    float handle_size = 10.0F)
{
    std::vector<glm::vec2> pts(path.begin(), path.end());

    std::vector<float> seg_lengths;
    seg_lengths.reserve(pts.size() > 0 ? pts.size() - 1 : 0);
    float total_len = 0.0F;
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        float l = glm::length(pts[i + 1] - pts[i]);
        seg_lengths.push_back(l);
        total_len += l;
    }

    Kinesis::AABB2D aabb { .min = pts.empty() ? glm::vec2(0.F) : pts[0],
        .max = pts.empty() ? glm::vec2(0.F) : pts[0] };
    for (const auto& p : pts) {
        aabb.min = glm::min(aabb.min, p);
        aabb.max = glm::max(aabb.max, p);
    }
    aabb = aabb.expanded(half_thickness);

    return [pts = std::move(pts),
               seg_lengths = std::move(seg_lengths),
               total_len,
               aabb,
               handle_buf = std::move(handle_buf),
               half_thickness,
               track_color,
               fill_color,
               handle_color,
               handle_size](float v, std::vector<uint8_t>& out, Element& el) {
        if (pts.size() < 2) {
            out.clear();
            return;
        }

        const float target = std::clamp(v, 0.0F, 1.0F) * total_len;

        glm::vec2 handle_pos = pts.front();
        float accumulated = 0.0F;
        size_t split_seg = 0;
        float split_t = 0.0F;
        for (size_t i = 0; i < seg_lengths.size(); ++i) {
            if (accumulated + seg_lengths[i] >= target || i + 1 == seg_lengths.size()) {
                split_t = seg_lengths[i] > 0.0F
                    ? (target - accumulated) / seg_lengths[i]
                    : 0.0F;
                split_t = std::clamp(split_t, 0.0F, 1.0F);
                handle_pos = glm::mix(pts[i], pts[i + 1], split_t);
                split_seg = i;
                break;
            }
            accumulated += seg_lengths[i];
        }

        std::vector<Kakshya::LineVertex> verts;
        verts.reserve(pts.size() * 4);

        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            verts.push_back({ .position = { pts[i].x, pts[i].y, 0.0F }, .color = track_color });
            verts.push_back({ .position = { pts[i + 1].x, pts[i + 1].y, 0.0F }, .color = track_color });
        }

        for (size_t i = 0; i < split_seg; ++i) {
            verts.push_back({ .position = { pts[i].x, pts[i].y, 0.0F }, .color = fill_color });
            verts.push_back({ .position = { pts[i + 1].x, pts[i + 1].y, 0.0F }, .color = fill_color });
        }
        if (split_t > 0.0F) {
            verts.push_back({ .position = { pts[split_seg].x, pts[split_seg].y, 0.0F }, .color = fill_color });
            verts.push_back({ .position = { handle_pos.x, handle_pos.y, 0.0F }, .color = fill_color });
        }

        write_verts(out, verts);

        el.bounds_hint = aabb;
        el.contains = Kinesis::stroke_bounds(pts, half_thickness);

        if (handle_buf) {
            Kakshya::PointVertex hv {
                .position = { handle_pos.x, handle_pos.y, 0.0F },
                .color = handle_color,
                .size = handle_size,
            };
            std::vector<uint8_t> hbytes(sizeof(hv));
            std::memcpy(hbytes.data(), &hv, sizeof(hv));
            handle_buf->submit(hbytes);
        }
    };
}

} // namespace MayaFlux::Portal::Forma::Geometry
