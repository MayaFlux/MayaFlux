#include "Plot.hpp"

#include "MayaFlux/Core/Backends/Graphics/Vulkan/VKImage.hpp"
#include "MayaFlux/Kakshya/Region/RegionGroup.hpp"
#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

namespace MayaFlux::Portal::Forma::Plot {

using Role = Kakshya::DataDimension::Role;
using Nodes::LineVertex;
using Nodes::MeshVertex;
using Nodes::PointVertex;
using Nodes::TextureQuadVertex;

// =============================================================================
// series_by_role
// =============================================================================

std::vector<std::span<const double>> series_by_role(
    const Kakshya::PlotContainer& container,
    Role role)
{
    const auto& dims = container.get_structure().dimensions;
    const auto& data = container.get_processed_data();

    std::vector<std::span<const double>> result;
    for (size_t i = 0; i < dims.size() && i < data.size(); ++i) {
        if (dims[i].role != role)
            continue;
        const auto* vec = std::get_if<std::vector<double>>(&data[i]);
        if (!vec || vec->empty())
            continue;
        result.emplace_back(vec->data(), vec->size());
    }
    return result;
}

// =============================================================================
// data_range / apply_auto_scale
// =============================================================================

std::pair<float, float> data_range(std::span<const double> series)
{
    if (series.empty())
        return { 0.F, 1.F };

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (double v : series) {
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
    }
    if (lo == hi)
        hi = lo + 1.0;
    return { static_cast<float>(lo), static_cast<float>(hi) };
}

void apply_auto_scale(AxisRange& range,
    const std::vector<std::span<const double>>& series)
{
    if (!range.auto_scaling || series.empty())
        return;
    if (range.scale_predicate && !range.scale_predicate())
        return;

    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    for (const auto& s : series) {
        auto [slo, shi] = data_range(s);
        lo = std::min(lo, slo);
        hi = std::max(hi, shi);
    }
    range.min = lo;
    range.max = hi;
}

// =============================================================================
// palette_color
// =============================================================================

glm::vec3 palette_color(const std::vector<glm::vec3>& palette,
    size_t index) noexcept
{
    if (palette.empty())
        return glm::vec3(1.F);
    return palette[index % palette.size()];
}

// =============================================================================
// Helpers
// =============================================================================

namespace {

    template <typename V>
    void push_vertex(std::vector<uint8_t>& out, const V& v)
    {
        const auto* src = reinterpret_cast<const uint8_t*>(&v);
        out.insert(out.end(), src, src + sizeof(V));
    }

    std::vector<float> uniform_x(size_t n, const AxisRange& x_range)
    {
        std::vector<float> xs(n);
        if (n == 1) {
            xs[0] = x_range.to_ndc((x_range.min + x_range.max) * 0.5F);
            return xs;
        }
        for (size_t i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n - 1);
            const float v = x_range.min + t * (x_range.max - x_range.min);
            xs[i] = x_range.to_ndc(v);
        }
        return xs;
    }

} // namespace

// =============================================================================
// waveform
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> waveform(
    AxisRange x_range,
    AxisRange y_range,
    AxisRange z_range,
    const std::vector<glm::vec3>& palette,
    float thickness)
{
    return [x_range, y_range, z_range, palette, thickness](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        auto y_series = series_by_role(*container, Role::SPATIAL_Y);
        if (y_series.empty())
            y_series = series_by_role(*container, Role::TIME);
        if (y_series.empty())
            return;

        const auto x_series = series_by_role(*container, Role::SPATIAL_X);
        const auto z_series = series_by_role(*container, Role::SPATIAL_Z);

        apply_auto_scale(x_range, x_series.empty() ? y_series : x_series);
        apply_auto_scale(y_range, y_series);
        apply_auto_scale(z_range, z_series);

        out.clear();

        for (size_t si = 0; si < y_series.size(); ++si) {
            const auto& ys = y_series[si];
            const size_t n = ys.size();
            if (n == 0)
                continue;

            const glm::vec3 color = palette_color(palette, si);

            const bool has_x = si < x_series.size() && x_series[si].size() == n;
            const bool has_z = si < z_series.size() && z_series[si].size() == n;

            std::vector<float> xs;
            if (has_x) {
                xs.reserve(n);
                for (double v : x_series[si])
                    xs.push_back(x_range.to_ndc(static_cast<float>(v)));
            } else {
                xs = uniform_x(n, x_range);
            }

            for (size_t i = 0; i < n; ++i) {
                LineVertex v;
                v.position = glm::vec3(
                    xs[i],
                    y_range.to_ndc(static_cast<float>(ys[i])),
                    has_z ? z_range.to_ndc(static_cast<float>(z_series[si][i])) : 0.F);
                v.color = color;
                v.thickness = thickness;
                push_vertex(out, v);
            }

            // Degenerate separator between runs: repeat last vertex twice
            // with zero thickness so the LINE_STRIP draws no segment between runs.
            if (si + 1 < y_series.size()) {
                const float last_z = has_z
                    ? z_range.to_ndc(static_cast<float>(z_series[si][n - 1]))
                    : 0.F;
                LineVertex sep;
                sep.position = glm::vec3(
                    xs[n - 1],
                    y_range.to_ndc(static_cast<float>(ys[n - 1])),
                    last_z);
                sep.color = color;
                sep.thickness = 0.F;
                push_vertex(out, sep);
                push_vertex(out, sep);
            }
        }
    };
}

// =============================================================================
// scatter
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> scatter(
    AxisRange x_range,
    AxisRange y_range,
    AxisRange z_range,
    const std::vector<glm::vec3>& palette,
    float point_size)
{
    return [x_range, y_range, z_range, palette, point_size](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        const auto x_series = series_by_role(*container, Role::SPATIAL_X);
        auto y_series = series_by_role(*container, Role::SPATIAL_Y);
        const auto z_series = series_by_role(*container, Role::SPATIAL_Z);
        const auto c_series = series_by_role(*container, Role::COLOR);

        if (y_series.empty())
            return;

        apply_auto_scale(x_range, x_series);
        apply_auto_scale(y_range, y_series);
        apply_auto_scale(z_range, z_series);

        out.clear();

        for (size_t si = 0; si < y_series.size(); ++si) {
            const auto& ys = y_series[si];
            const size_t n = ys.size();
            if (n == 0)
                continue;

            const glm::vec3 base_color = palette_color(palette, si);

            // Reuse last X series if fewer X series than Y series.
            const bool has_x = !x_series.empty();
            const auto& xs = has_x
                ? x_series[std::min(si, x_series.size() - 1)]
                : std::span<const double> {};

            const bool has_z = si < z_series.size() && z_series[si].size() == n;
            const bool has_c = si < c_series.size() && c_series[si].size() == n;

            for (size_t i = 0; i < n; ++i) {
                PointVertex v;

                v.position.x = has_x
                    ? x_range.to_ndc(static_cast<float>(xs[std::min(i, xs.size() - 1)]))
                    : x_range.to_ndc(x_range.min
                          + static_cast<float>(i)
                              / static_cast<float>(std::max<size_t>(n - 1, 1))
                              * (x_range.max - x_range.min));

                v.position.y = y_range.to_ndc(static_cast<float>(ys[i]));

                v.position.z = has_z
                    ? z_range.to_ndc(static_cast<float>(z_series[si][i]))
                    : 0.F;

                if (has_c) {
                    const float t = y_range.normalise(
                        static_cast<float>(c_series[si][i]));
                    v.color = glm::mix(glm::vec3(1.F), base_color, t);
                } else {
                    v.color = base_color;
                }

                v.size = point_size;
                push_vertex(out, v);
            }
        }
    };
}

// =============================================================================
// bars
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> bars(
    AxisRange x_range,
    AxisRange y_range,
    const std::vector<glm::vec3>& palette)
{
    return [x_range, y_range, palette](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        auto f_series = series_by_role(*container, Role::FREQUENCY);
        if (f_series.empty())
            f_series = series_by_role(*container, Role::SPATIAL_Y);
        if (f_series.empty())
            return;

        apply_auto_scale(y_range, f_series);

        out.clear();

        for (size_t si = 0; si < f_series.size(); ++si) {
            const auto& series = f_series[si];
            const size_t n = series.size();
            if (n == 0)
                continue;

            const glm::vec3 color = palette_color(palette, si);

            const float bar_w = (x_range.max - x_range.min)
                / static_cast<float>(n);
            const float y_base = y_range.to_ndc(0.F);

            for (size_t i = 0; i < n; ++i) {
                const float x_left = x_range.to_ndc(
                    x_range.min + static_cast<float>(i) * bar_w);
                const float x_right = x_range.to_ndc(
                    x_range.min + static_cast<float>(i + 1) * bar_w);
                const float y_top = y_range.to_ndc(
                    static_cast<float>(series[i]));

                const auto emit = [&](float x, float y) {
                    MeshVertex v;
                    v.position = glm::vec3(x, y, 0.F);
                    v.color = color;
                    push_vertex(out, v);
                };

                // Two triangles per bar (TRIANGLE_LIST).
                emit(x_left, y_base);
                emit(x_right, y_base);
                emit(x_left, y_top);
                emit(x_right, y_base);
                emit(x_right, y_top);
                emit(x_left, y_top);
            }
        }
    };
}

// =============================================================================
// background
// =============================================================================

GeometryFn<float> background(
    Kinesis::AABB2D bounds,
    glm::vec3 color,
    const std::shared_ptr<Core::VKImage>& texture)
{
    return [bounds, color, texture](
               float,
               std::vector<uint8_t>& out,
               Element& el) {
        out.clear();

        if (texture) {
            // Textured quad — four TextureQuadVertex as TRIANGLE_STRIP.
            // UV origin bottom-left matching Vulkan image layout.
            const std::array<TextureQuadVertex, 4> verts = { {
                { .position = { bounds.min.x, bounds.min.y, 0.F }, .texcoord = { 0.F, 1.F } },
                { .position = { bounds.max.x, bounds.min.y, 0.F }, .texcoord = { 1.F, 1.F } },
                { .position = { bounds.min.x, bounds.max.y, 0.F }, .texcoord = { 0.F, 0.F } },
                { .position = { bounds.max.x, bounds.max.y, 0.F }, .texcoord = { 1.F, 0.F } },
            } };
            const auto* src = reinterpret_cast<const uint8_t*>(verts.data());
            out.insert(out.end(), src, src + sizeof(verts));
        } else {
            // Solid color quad — four MeshVertex as TRIANGLE_STRIP.
            const std::array<MeshVertex, 4> verts = { {
                { .position = { bounds.min.x, bounds.min.y, 0.F }, .color = color },
                { .position = { bounds.max.x, bounds.min.y, 0.F }, .color = color },
                { .position = { bounds.min.x, bounds.max.y, 0.F }, .color = color },
                { .position = { bounds.max.x, bounds.max.y, 0.F }, .color = color },
            } };
            const auto* src = reinterpret_cast<const uint8_t*>(verts.data());
            out.insert(out.end(), src, src + sizeof(verts));
        }

        el.bounds_hint = bounds;
    };
}

} // namespace MayaFlux::Portal::Forma::Plot
