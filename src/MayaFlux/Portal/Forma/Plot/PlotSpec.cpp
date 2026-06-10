#include "PlotSpec.hpp"

#include "MayaFlux/Kakshya/Source/PlotContainer.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Geometry.hpp"

namespace MayaFlux::Portal::Forma::Plot {

// =============================================================================
// series_by_role
// =============================================================================

std::vector<std::span<const double>> series_by_role(
    const Kakshya::PlotContainer& container,
    Kakshya::DataDimension::Role role)
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
            const uint32_t stride = Kakshya::VertexLayout::for_textured_quad().stride_bytes;
            const std::array<std::pair<glm::vec3, glm::vec2>, 4> verts = { {
                { { bounds.min.x, bounds.min.y, 0.F }, { 0.F, 1.F } },
                { { bounds.max.x, bounds.min.y, 0.F }, { 1.F, 1.F } },
                { { bounds.min.x, bounds.max.y, 0.F }, { 0.F, 0.F } },
                { { bounds.max.x, bounds.max.y, 0.F }, { 1.F, 0.F } },
            } };
            out.resize(static_cast<size_t>(4) * stride);
            for (size_t i = 0; i < 4; ++i) {
                uint8_t* v = out.data() + i * stride;
                std::memcpy(v, &verts[i].first, 12);
                std::memcpy(v + 12, &verts[i].second, 8);
            }
        } else {
            const uint32_t stride = Kakshya::VertexLayout::for_meshes().stride_bytes;
            constexpr glm::vec3 k_normal { 0.F, 0.F, 1.F };
            constexpr glm::vec3 k_tangent { 1.F, 0.F, 0.F };
            constexpr glm::vec2 k_uv { 0.F, 0.F };
            constexpr float k_weight = 0.F;

            const std::array<glm::vec3, 4> positions = { {
                { bounds.min.x, bounds.min.y, 0.F },
                { bounds.max.x, bounds.min.y, 0.F },
                { bounds.min.x, bounds.max.y, 0.F },
                { bounds.max.x, bounds.max.y, 0.F },
            } };

            out.resize(static_cast<size_t>(4) * stride, 0);
            for (size_t i = 0; i < 4; ++i) {
                uint8_t* v = out.data() + i * stride;
                std::memcpy(v, &positions[i], 12);
                std::memcpy(v + 12, &color, 12);
                std::memcpy(v + 24, &k_weight, 4);
                std::memcpy(v + 28, &k_uv, 8);
                std::memcpy(v + 36, &k_normal, 12);
                std::memcpy(v + 48, &k_tangent, 12);
            }
        }

        el.bounds_hint = bounds;
    };
}

std::vector<Kakshya::LineVertex> plot_grid(
    Kinesis::AABB2D bounds,
    uint32_t x_divisions,
    uint32_t y_divisions,
    glm::vec3 color,
    float thickness)
{
    std::vector<Kakshya::LineVertex> out;
    out.reserve((static_cast<size_t>(x_divisions + y_divisions)) * 2);

    auto lv = [&](glm::vec2 p) {
        return Kakshya::LineVertex {
            .position = { p.x, p.y, 0.F },
            .color = color,
            .thickness = thickness,
        };
    };

    for (uint32_t i = 0; i < x_divisions; ++i) {
        const float t = (x_divisions > 1)
            ? static_cast<float>(i) / static_cast<float>(x_divisions - 1)
            : 0.5F;
        const float x = bounds.min.x + t * bounds.width();
        out.push_back(lv({ x, bounds.min.y }));
        out.push_back(lv({ x, bounds.max.y }));
    }

    for (uint32_t i = 0; i < y_divisions; ++i) {
        const float t = (y_divisions > 1)
            ? static_cast<float>(i) / static_cast<float>(y_divisions - 1)
            : 0.5F;
        const float y = bounds.min.y + t * bounds.height();
        out.push_back(lv({ bounds.min.x, y }));
        out.push_back(lv({ bounds.max.x, y }));
    }

    return out;
}

GeometryFn<float> plot_cursor(
    Kinesis::AABB2D bounds,
    bool vertical,
    glm::vec3 color,
    float thickness)
{
    return [bounds, vertical, color, thickness](
               float v, std::vector<uint8_t>& out, Element& el) {
        const float t = std::clamp(v, 0.F, 1.F);
        using V = Kakshya::LineVertex;
        std::array<V, 2> verts;
        if (vertical) {
            const float x = bounds.min.x + t * bounds.width();
            verts = { {
                { .position = { x, bounds.min.y, 0.F }, .color = color, .thickness = thickness },
                { .position = { x, bounds.max.y, 0.F }, .color = color, .thickness = thickness },
            } };
            el.bounds_hint = Kinesis::AABB2D {
                .min = { x - 0.01F, bounds.min.y },
                .max = { x + 0.01F, bounds.max.y },
            };
        } else {
            const float y = bounds.min.y + t * bounds.height();
            verts = { {
                { .position = { bounds.min.x, y, 0.F }, .color = color, .thickness = thickness },
                { .position = { bounds.max.x, y, 0.F }, .color = color, .thickness = thickness },
            } };
            el.bounds_hint = Kinesis::AABB2D {
                .min = { bounds.min.x, y - 0.01F },
                .max = { bounds.max.x, y + 0.01F },
            };
        }
        Geometry::write_verts(out, verts);
    };
}

} // namespace MayaFlux::Portal::Forma::Plot
