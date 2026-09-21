#include "FormFactory.hpp"

#include "MayaFlux/Kinesis/Geometry2D.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"

namespace MayaFlux::Portal::Forma::Geometry {

Form<float> horizontal_fader(
    Kinesis::AABB2D bounds,
    float handle_w,
    glm::vec3 track_color,
    glm::vec3 handle_color,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<float> fn = [bounds, handle_w, track_color, handle_color, live_offset](
                               float v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = bounds.translated(shift);

        float x = live.min.x + v * (live.width() - handle_w);
        float yt = live.min.y + live.height() * 0.35F;
        float yb = live.min.y + live.height() * 0.65F;

        Kinesis::AABB2D track { .min = glm::vec2(live.min.x, yt), .max = glm::vec2(live.max.x, yb) };
        Kinesis::AABB2D handle { .min = glm::vec2(x, live.min.y), .max = glm::vec2(x + handle_w, live.max.y) };

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(track, track_color));
        auto herts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(handle, handle_color));
        verts.insert(verts.end(), herts.begin(), herts.end());

        write_verts(out, verts);

        el.bounds_hint = handle;
        el.contains = {};
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP,
        static_cast<size_t>(8) * sizeof(Kakshya::MeshVertex),
        drag_with<float>(Kinesis::axis_fraction(bounds, handle_w, true, live_offset)) };
}

Form<float> vertical_fader(
    Kinesis::AABB2D bounds,
    float handle_h,
    glm::vec3 track_color,
    glm::vec3 handle_color,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<float> fn = [bounds, handle_h, track_color, handle_color, live_offset](
                               float v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = bounds.translated(shift);

        const float y = live.min.y + v * (live.height() - handle_h);
        const float xl = live.min.x + live.width() * 0.35F;
        const float xr = live.min.x + live.width() * 0.65F;

        const Kinesis::AABB2D track { .min = { xl, live.min.y }, .max = { xr, live.max.y } };
        const Kinesis::AABB2D handle { .min = { live.min.x, y }, .max = { live.max.x, y + handle_h } };

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(track, track_color));
        auto herts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(handle, handle_color));
        verts.insert(verts.end(), herts.begin(), herts.end());

        write_verts(out, verts);

        el.bounds_hint = handle;
        el.contains = {};
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP,
        static_cast<size_t>(8) * sizeof(Kakshya::MeshVertex),
        drag_with<float>(Kinesis::axis_fraction(bounds, handle_h, false, live_offset)) };
}

Form<float> radial(
    Kinesis::AABB2D region,
    float angle_start,
    float angle_end,
    glm::vec3 color,
    const std::function<glm::vec2()>& live_offset)
{
    const glm::vec2 center = region.center();
    const float radius = std::min(region.width(), region.height()) * 0.5F;

    GeometryFn<float> fn = [center, radius, angle_start, angle_end, color, live_offset](
                               float v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const glm::vec2 live_center = center + shift;

        const float angle = angle_start + v * (angle_end - angle_start);
        const glm::vec2 tip = live_center + radius * glm::vec2(std::cos(angle), std::sin(angle));

        using V = Kakshya::LineVertex;
        const std::vector<V> verts = {
            { .position = { live_center.x, live_center.y, 0 }, .color = color },
            { .position = { tip.x, tip.y, 0 }, .color = color },
        };

        write_verts(out, verts);

        el.bounds_hint = Kinesis::AABB2D::from_ndc(live_center, glm::vec2(radius));
        el.contains = Kinesis::circular_bounds(live_center, radius);
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::LINE_LIST,
        static_cast<size_t>(2) * sizeof(Kakshya::LineVertex),
        drag_with<float>(Kinesis::angle_fraction(center, angle_start, angle_end, live_offset)) };
}

Form<glm::vec2> point(
    glm::vec3 color,
    float size,
    float hit_radius)
{
    GeometryFn<glm::vec2> fn = [color, size, hit_radius](glm::vec2 pos, std::vector<uint8_t>& out, Element& el) {
        write_verts(out, Kakshya::PointVertex {
                             .position = { pos.x, pos.y, 0.0F },
                             .color = color,
                             .size = size,
                         });
        el.bounds_hint = Kinesis::AABB2D::from_ndc(pos, glm::vec2(hit_radius));
        el.contains = Kinesis::circular_bounds(pos, hit_radius);
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::POINT_LIST,
        sizeof(Kakshya::PointVertex),
        follow_move() };
}

Form<glm::vec2> position_picker(
    Kinesis::AABB2D bounds,
    glm::vec3 color,
    float size,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<glm::vec2> fn = [bounds, color, size, live_offset](
                                   glm::vec2 v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = bounds.translated(shift);

        float x = live.min.x + v.x * live.width();
        float y = live.min.y + v.y * live.height();

        using V = Kakshya::PointVertex;
        std::vector<V> verts = {
            { .position = { x, y, 0 }, .color = color, .size = size },
        };

        write_verts(out, verts);

        el.bounds_hint = live;
        el.contains = Kinesis::polygon_bounds(std::span<const glm::vec2> {
            std::array<glm::vec2, 4> {
                live.min,
                glm::vec2(live.max.x, live.min.y),
                live.max,
                glm::vec2(live.min.x, live.max.y) } });
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::POINT_LIST,
        sizeof(Kakshya::PointVertex),
        drag_with<glm::vec2>(Kinesis::unit_square(bounds, live_offset)) };
}

Form<float> stroke_slider(
    std::span<const glm::vec2> path,
    std::shared_ptr<Buffers::FormaBuffer> handle_buf,
    float half_thickness,
    glm::vec3 track_color,
    glm::vec3 fill_color,
    glm::vec3 handle_color,
    float handle_size,
    const std::function<glm::vec2()>& live_offset)
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

    auto project = Kinesis::path_fraction(path, live_offset);
    const size_t cap = pts.size() * 4 * sizeof(Kakshya::LineVertex);

    GeometryFn<float> fn = [pts = std::move(pts),
                               seg_lengths = std::move(seg_lengths),
                               total_len,
                               aabb,
                               handle_buf = std::move(handle_buf),
                               half_thickness,
                               track_color,
                               fill_color,
                               handle_color,
                               handle_size,
                               live_offset](float v, std::vector<uint8_t>& out, Element& el) {
        if (pts.size() < 2) {
            out.clear();
            return;
        }

        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);

        const float target = std::clamp(v, 0.0F, 1.0F) * total_len;

        // Arc-length interpolation is translation-invariant. It is computed
        // in pts' own (unshifted) frame, and the shift is applied once to
        // the result.
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
        handle_pos += shift;

        std::vector<glm::vec2> live_pts;
        live_pts.reserve(pts.size());
        for (const auto& p : pts)
            live_pts.push_back(p + shift);

        auto verts = Kinesis::polyline(live_pts, track_color);

        auto fill = Kinesis::polyline(
            std::span<const glm::vec2>(live_pts).subspan(0, split_seg + 1),
            fill_color);
        verts.insert(verts.end(), fill.begin(), fill.end());

        if (split_t > 0.0F) {
            verts.push_back({ .position = { live_pts[split_seg].x, live_pts[split_seg].y, 0.0F }, .color = fill_color });
            verts.push_back({ .position = { handle_pos.x, handle_pos.y, 0.0F }, .color = fill_color });
        }

        write_verts(out, verts);

        el.bounds_hint = aabb.translated(shift);
        el.contains = Kinesis::stroke_bounds(live_pts, half_thickness);

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

    return { std::move(fn),
        Graphics::PrimitiveTopology::LINE_LIST,
        cap,
        drag_with<float>(std::move(project)) };
}

Form<bool> toggle(
    Kinesis::AABB2D region,
    glm::vec3 color_off,
    glm::vec3 color_on,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<bool> fn = [region, color_off, color_on, live_offset](
                              bool v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = region.translated(shift);

        write_verts(out, Kakshya::to_mesh_vertices(Kinesis::filled_rect(live, v ? color_on : color_off)));
        el.bounds_hint = live;
        el.contains = {};
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP,
        static_cast<size_t>(4) * sizeof(Kakshya::MeshVertex),
        press_flip() };
}

Form<float> level_meter(
    Kinesis::AABB2D bounds,
    bool horizontal,
    glm::vec3 fill_color,
    glm::vec3 track_color,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<float> fn = [bounds, horizontal, fill_color, track_color, live_offset](
                               float v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = bounds.translated(shift);

        const float t = std::clamp(v, 0.F, 1.F);

        Kinesis::AABB2D fill {}, remainder {};
        if (horizontal) {
            const float split = live.min.x + t * live.width();
            fill = { .min = live.min, .max = { split, live.max.y } };
            remainder = { .min = { split, live.min.y }, .max = live.max };
        } else {
            const float split = live.min.y + t * live.height();
            fill = { .min = live.min, .max = { live.max.x, split } };
            remainder = { .min = { live.min.x, split }, .max = live.max };
        }

        auto verts = Kakshya::to_mesh_vertices(Kinesis::filled_rect(fill, fill_color));
        auto rest = Kakshya::to_mesh_vertices(Kinesis::filled_rect(remainder, track_color));
        verts.insert(verts.end(), rest.begin(), rest.end());

        write_verts(out, verts);

        el.bounds_hint = live;
        el.contains = {};
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP,
        static_cast<size_t>(8) * sizeof(Kakshya::MeshVertex) };
}

Form<glm::vec2> crosshair(
    float arm_len,
    glm::vec3 color,
    float thickness,
    float hit_radius)
{
    GeometryFn<glm::vec2> fn = [arm_len, color, thickness, hit_radius](
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

    return { std::move(fn),
        Graphics::PrimitiveTopology::LINE_LIST,
        static_cast<size_t>(4) * sizeof(Kakshya::LineVertex),
        follow_move() };
}

Form<std::vector<float>> drawable_canvas(
    Kinesis::AABB2D bounds,
    glm::vec3 color,
    float thickness,
    const std::function<glm::vec2()>& live_offset)
{
    GeometryFn<std::vector<float>> fn = [bounds, color, thickness, live_offset](
                                            const std::vector<float>& v, std::vector<uint8_t>& out, Element& el) {
        const glm::vec2 shift = live_offset ? live_offset() : glm::vec2(0.F);
        const Kinesis::AABB2D live = bounds.translated(shift);

        if (v.size() < 2) {
            out.clear();
            el.bounds_hint = live;
            el.contains = Kinesis::polygon_bounds(std::span<const glm::vec2> {
                std::array<glm::vec2, 4> {
                    live.min,
                    glm::vec2(live.max.x, live.min.y),
                    live.max,
                    glm::vec2(live.min.x, live.max.y) } });
            return;
        }

        const auto n = v.size();
        const float x_step = live.width() / static_cast<float>(n - 1);

        std::vector<Kakshya::LineVertex> verts;
        verts.reserve((n - 1) * 2);

        for (size_t i = 0; i + 1 < n; ++i) {
            const float xa = live.min.x + static_cast<float>(i) * x_step;
            const float xb = live.min.x + static_cast<float>(i + 1) * x_step;
            const float ya = live.min.y + std::clamp(v[i], 0.F, 1.F) * live.height();
            const float yb = live.min.y + std::clamp(v[i + 1], 0.F, 1.F) * live.height();

            verts.push_back({ .position = { xa, ya, 0.F }, .color = color, .thickness = thickness });
            verts.push_back({ .position = { xb, yb, 0.F }, .color = color, .thickness = thickness });
        }

        write_verts(out, verts);

        el.bounds_hint = live;
        el.contains = Kinesis::polygon_bounds(std::span<const glm::vec2> {
            std::array<glm::vec2, 4> {
                live.min,
                glm::vec2(live.max.x, live.min.y),
                live.max,
                glm::vec2(live.min.x, live.max.y) } });
    };

    Form<std::vector<float>> form { std::move(fn) };
    form.topology = Graphics::PrimitiveTopology::LINE_LIST;
    form.wire = paint_over(bounds);
    return form;
}

void wire_canvas_drag(
    Context& ctx,
    uint32_t id,
    const std::shared_ptr<MappedState<std::vector<float>>>& state,
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

Form<glm::vec2> scroll_indicator(
    Kinesis::AABB2D track,
    glm::vec3 color)
{
    GeometryFn<glm::vec2> fn = [track, color](
                                   glm::vec2 extent, std::vector<uint8_t>& out, Element& el) {
        const float len = std::clamp(extent.y, 0.F, 1.F) * track.height();
        const float top = track.max.y - std::clamp(extent.x, 0.F, 1.F) * (track.height() - len);
        const Kinesis::AABB2D bar { .min = { track.min.x, top - len }, .max = { track.max.x, top } };

        write_verts(out, Kakshya::to_mesh_vertices(Kinesis::filled_rect(bar, color)));

        el.bounds_hint = track;
        el.contains = {};
    };

    return { std::move(fn),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP,
        static_cast<size_t>(4) * sizeof(Kakshya::MeshVertex) };
}

} // namespace MayaFlux::Portal::Forma::Geometry
