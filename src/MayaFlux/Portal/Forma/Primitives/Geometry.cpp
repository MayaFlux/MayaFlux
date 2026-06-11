#include "Geometry.hpp"

#include "MayaFlux/Kinesis/Geometry2D.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Portal/Forma/Context.hpp"

namespace MayaFlux::Portal::Forma::Geometry {

GeometryFn<float> horizontal_fader(
    Kinesis::AABB2D bounds,
    float handle_w,
    glm::vec3 track_color,
    glm::vec3 handle_color)
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

GeometryFn<float> vertical_fader(
    Kinesis::AABB2D bounds,
    float handle_h,
    glm::vec3 track_color,
    glm::vec3 handle_color)
{
    return [bounds, handle_h, track_color, handle_color](
               float v, std::vector<uint8_t>& out, Element& el) {
        const float y = bounds.min.y + v * (bounds.height() - handle_h);
        const float xl = bounds.min.x + bounds.width() * 0.35F;
        const float xr = bounds.min.x + bounds.width() * 0.65F;

        const Kinesis::AABB2D track { .min = { xl, bounds.min.y }, .max = { xr, bounds.max.y } };
        const Kinesis::AABB2D handle { .min = { bounds.min.x, y }, .max = { bounds.max.x, y + handle_h } };

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(track, track_color));
        auto herts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(handle, handle_color));
        verts.insert(verts.end(), herts.begin(), herts.end());

        write_verts(out, verts);

        el.bounds_hint = handle;
        el.contains = {};
    };
}

GeometryFn<float> radial(
    glm::vec2 center,
    float radius,
    float angle_start,
    float angle_end,
    glm::vec3 color)
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

GeometryFn<glm::vec2> point(
    glm::vec3 color,
    float size,
    float hit_radius)
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

GeometryFn<glm::vec2> position_picker(
    Kinesis::AABB2D bounds,
    glm::vec3 color,
    float size)
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

GeometryFn<float> stroke_slider(
    std::span<const glm::vec2> path,
    std::shared_ptr<Buffers::FormaBuffer> handle_buf,
    float half_thickness,
    glm::vec3 track_color,
    glm::vec3 fill_color,
    glm::vec3 handle_color,
    float handle_size)
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

        auto verts = Kinesis::polyline(pts, track_color);

        auto fill = Kinesis::polyline(
            std::span<const glm::vec2>(pts).subspan(0, split_seg + 1),
            fill_color);
        verts.insert(verts.end(), fill.begin(), fill.end());

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

GeometryFn<bool> toggle(
    Kinesis::AABB2D region,
    glm::vec3 color_off,
    glm::vec3 color_on)
{
    return [region, color_off, color_on](
               bool v, std::vector<uint8_t>& out, Element& el) {
        write_verts(out, Kakshya::to_mesh_vertices(Kinesis::filled_rect(region, v ? color_on : color_off)));
        el.bounds_hint = region;
        el.contains = {};
    };
}

GeometryFn<float> level_meter(
    Kinesis::AABB2D bounds,
    bool horizontal,
    glm::vec3 fill_color,
    glm::vec3 track_color)
{
    return [bounds, horizontal, fill_color, track_color](
               float v, std::vector<uint8_t>& out, Element& el) {
        const float t = std::clamp(v, 0.F, 1.F);

        Kinesis::AABB2D fill {}, remainder {};
        if (horizontal) {
            const float split = bounds.min.x + t * bounds.width();
            fill = { .min = bounds.min, .max = { split, bounds.max.y } };
            remainder = { .min = { split, bounds.min.y }, .max = bounds.max };
        } else {
            const float split = bounds.min.y + t * bounds.height();
            fill = { .min = bounds.min, .max = { bounds.max.x, split } };
            remainder = { .min = { bounds.min.x, split }, .max = bounds.max };
        }

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(fill, fill_color));
        auto rest = Kakshya::to_mesh_vertices(Kinesis::filled_rect(remainder, track_color));
        verts.insert(verts.end(), rest.begin(), rest.end());

        write_verts(out, verts);

        el.bounds_hint = bounds;
        el.contains = {};
    };
}

GeometryFn<glm::vec2> crosshair(
    float arm_len,
    glm::vec3 color,
    float thickness,
    float hit_radius)
{
    return [arm_len, color, thickness, hit_radius](
               glm::vec2 pos, std::vector<uint8_t>& out, Element& el) {
        using V = Kakshya::LineVertex;
        const std::array<V, 4> verts { {
            { .position = { pos.x - arm_len, pos.y, 0.F }, .color = color, .thickness = thickness },
            { .position = { pos.x + arm_len, pos.y, 0.F }, .color = color, .thickness = thickness },
            { .position = { pos.x, pos.y - arm_len, 0.F }, .color = color, .thickness = thickness },
            { .position = { pos.x, pos.y + arm_len, 0.F }, .color = color, .thickness = thickness },
        } };
        write_verts(out, verts);
        el.bounds_hint = Kinesis::AABB2D::from_ndc(pos, glm::vec2(arm_len));
        el.contains = Kinesis::circular_bounds(pos, hit_radius);
    };
}

GeometryFn<std::vector<float>> drawable_canvas(
    Kinesis::AABB2D bounds,
    glm::vec3 color,
    float thickness)
{
    return [bounds, color, thickness](
               const std::vector<float>& v, std::vector<uint8_t>& out, Element& el) {
        if (v.size() < 2) {
            out.clear();
            el.bounds_hint = bounds;
            el.contains = Kinesis::polygon_bounds(std::span<const glm::vec2> {
                std::array<glm::vec2, 4> {
                    bounds.min,
                    glm::vec2(bounds.max.x, bounds.min.y),
                    bounds.max,
                    glm::vec2(bounds.min.x, bounds.max.y) } });
            return;
        }

        const auto n = v.size();
        const float x_step = bounds.width() / static_cast<float>(n - 1);

        std::vector<Kakshya::LineVertex> verts;
        verts.reserve((n - 1) * 2);

        for (size_t i = 0; i + 1 < n; ++i) {
            const float xa = bounds.min.x + static_cast<float>(i) * x_step;
            const float xb = bounds.min.x + static_cast<float>(i + 1) * x_step;
            const float ya = bounds.min.y + std::clamp(v[i], 0.F, 1.F) * bounds.height();
            const float yb = bounds.min.y + std::clamp(v[i + 1], 0.F, 1.F) * bounds.height();

            verts.push_back({ .position = { xa, ya, 0.F }, .color = color, .thickness = thickness });
            verts.push_back({ .position = { xb, yb, 0.F }, .color = color, .thickness = thickness });
        }

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

void wire_canvas_drag(
    Context& ctx,
    uint32_t id,
    std::shared_ptr<MappedState<std::vector<float>>> state,
    Kinesis::AABB2D bounds)
{
    struct DragState {
        std::optional<size_t> prev_index;
    };
    auto ds = std::make_shared<DragState>();

    ctx.on_drag(id, IO::MouseButtons::Left,
        [state, bounds, ds](uint32_t, glm::vec2 ndc) {
            auto& v = state->value;
            if (v.empty())
                return;

            const float t = (ndc.x - bounds.min.x) / bounds.width();
            const float a = (ndc.y - bounds.min.y) / bounds.height();
            const size_t n = v.size();
            const size_t idx = static_cast<size_t>(
                std::clamp(t, 0.F, 1.F) * static_cast<float>(n - 1));
            const float amp = std::clamp(a, 0.F, 1.F);

            if (ds->prev_index && *ds->prev_index != idx) {
                const size_t lo = std::min(*ds->prev_index, idx);
                const size_t hi = std::max(*ds->prev_index, idx);
                const float v0 = v[*ds->prev_index];
                const auto span = static_cast<float>(hi - lo);
                for (size_t i = lo; i <= hi; ++i) {
                    const float f = span > 0.F
                        ? static_cast<float>(i - lo) / span
                        : 1.F;
                    v[i] = glm::mix(v0, amp, f);
                }
            } else {
                v[idx] = amp;
            }

            ds->prev_index = idx;
            ++state->version;
        });

    ctx.on_release(id, IO::MouseButtons::Left, [ds](uint32_t, glm::vec2) {
        ds->prev_index = std::nullopt;
    });
}

} // namespace MayaFlux::Portal::Forma::Geometry
