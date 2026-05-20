#include "SeriesBuilder.hpp"

#include "Plot.hpp"

namespace MayaFlux::Portal::Forma::Plot {

// =============================================================================
// Internal helpers
// =============================================================================

namespace {

    using Role = Kakshya::DataDimension::Role;

    /**
     * @brief Collect all series from the container matching any role in @p mapping.
     */
    std::vector<std::span<const double>> series_for_mapping(
        const Kakshya::PlotContainer& container,
        const SeriesBuilder::AxisMapping& mapping)
    {
        std::vector<std::span<const double>> result;
        for (const auto& role : mapping.roles) {
            auto s = series_by_role(container, role);
            result.insert(result.end(), s.begin(), s.end());
        }
        return result;
    }

    /**
     * @brief Flatten all AxisMappings into a single merged AxisRange.
     */
    AxisRange merge_axis(std::vector<SeriesBuilder::AxisMapping>& mappings)
    {
        if (mappings.empty())
            return {};
        if (mappings.size() == 1)
            return mappings[0].range;

        AxisRange merged = mappings[0].range;
        for (size_t i = 1; i < mappings.size(); ++i) {
            const auto& r = mappings[i].range;
            merged.min = std::min(merged.min, r.min);
            merged.max = std::max(merged.max, r.max);
            if (r.auto_scaling)
                merged.auto_scaling = true;
            if (!merged.scale_predicate && r.scale_predicate)
                merged.scale_predicate = r.scale_predicate;
        }
        return merged;
    }

    /**
     * @brief Resolve color for a series index within a mapping.
     */
    glm::vec3 resolve_color(
        const SeriesBuilder::AxisMapping& mapping,
        const std::vector<glm::vec3>& global_palette,
        size_t series_index)
    {
        if (!mapping.palette.empty())
            return palette_color(mapping.palette, series_index);
        return palette_color(global_palette, series_index);
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
            xs[i] = x_range.to_ndc(x_range.min + t * (x_range.max - x_range.min));
        }
        return xs;
    }

    /** Vertex field offsets — derived from VertexLayout, never hardcoded.
     * All three vertex types share the 60-byte layout:
     *   offset  0 — position  vec3  (12)
     *   offset 12 — color     vec3  (12)
     *   offset 24 — scalar    float  (4)  [size / thickness / weight]
     *   offset 28 — uv        vec2   (8)
     *   offset 36 — normal    vec3  (12)
     *   offset 48 — tangent   vec3  (12)
     **/

    constexpr uint32_t k_stride = 60;
    constexpr uint32_t k_off_pos = 0;
    constexpr uint32_t k_off_color = 12;
    constexpr uint32_t k_off_scalar = 24;

    constexpr glm::vec3 k_default_normal { 0.F, 0.F, 1.F };
    constexpr glm::vec3 k_default_tangent { 1.F, 0.F, 0.F };
    constexpr glm::vec2 k_default_uv { 0.F, 0.F };

    void write_vertex(
        std::vector<uint8_t>& out,
        glm::vec3 pos,
        glm::vec3 color,
        float scalar)
    {
        const size_t base = out.size();
        out.resize(base + k_stride, 0);
        uint8_t* v = out.data() + base;
        std::memcpy(v + k_off_pos, &pos, 12);
        std::memcpy(v + k_off_color, &color, 12);
        std::memcpy(v + k_off_scalar, &scalar, 4);
        std::memcpy(v + 28, &k_default_uv, 8);
        std::memcpy(v + 36, &k_default_normal, 12);
        std::memcpy(v + 48, &k_default_tangent, 12);
    }

} // namespace

// =============================================================================
// WaveformBuilder::done
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> WaveformBuilder::done() const
{
    auto x_mappings = m_state.x_mappings();
    auto y_mappings = m_state.y_mappings();
    auto z_mappings = m_state.z_mappings();
    const auto global_palette = m_state.palette();
    const float thickness = m_thickness;

    return [x_mappings, y_mappings, z_mappings, global_palette, thickness](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        struct SeriesEntry {
            std::span<const double> data;
            size_t mapping_index;
            size_t index_within_mapping;
        };

        std::vector<SeriesEntry> y_entries;
        for (size_t mi = 0; mi < y_mappings.size(); ++mi) {
            auto series = series_for_mapping(*container, y_mappings[mi]);
            for (size_t si = 0; si < series.size(); ++si)
                y_entries.push_back({ .data = series[si], .mapping_index = mi, .index_within_mapping = si });
        }
        if (y_entries.empty())
            return;

        AxisRange x_range = merge_axis(x_mappings);
        AxisRange z_range = z_mappings.empty() ? AxisRange {} : merge_axis(z_mappings);

        std::vector<std::span<const double>> all_x, all_z;
        for (auto& m : x_mappings) {
            auto s = series_for_mapping(*container, m);
            all_x.insert(all_x.end(), s.begin(), s.end());
        }
        for (auto& m : z_mappings) {
            auto s = series_for_mapping(*container, m);
            all_z.insert(all_z.end(), s.begin(), s.end());
        }

        apply_auto_scale(x_range, all_x.empty() ? std::vector<std::span<const double>> { y_entries[0].data } : all_x);
        apply_auto_scale(z_range, all_z);

        out.clear();

        for (size_t ei = 0; ei < y_entries.size(); ++ei) {
            const auto& entry = y_entries[ei];
            const auto& ys = entry.data;
            const size_t n = ys.size();
            if (n == 0)
                continue;

            const glm::vec3 color = resolve_color(
                y_mappings[entry.mapping_index], global_palette, entry.index_within_mapping);

            auto y_range = y_mappings[entry.mapping_index].range;
            apply_auto_scale(y_range, { ys });

            const bool has_x = ei < all_x.size() && all_x[ei].size() == n;
            const bool has_z = ei < all_z.size() && all_z[ei].size() == n;

            const std::vector<float> xs = has_x
                ? [&] {
                      std::vector<float> v;
                      v.reserve(n);
                      for (double val : all_x[ei])
                          v.push_back(x_range.to_ndc(static_cast<float>(val)));
                      return v;
                  }()
                : uniform_x(n, x_range);

            for (size_t i = 0; i < n; ++i) {
                write_vertex(out,
                    { xs[i],
                        y_range.to_ndc(static_cast<float>(ys[i])),
                        has_z ? z_range.to_ndc(static_cast<float>(all_z[ei][i])) : 0.F },
                    color,
                    thickness);
            }

            if (ei + 1 < y_entries.size()) {
                const glm::vec3 sep_pos {
                    xs[n - 1],
                    y_range.to_ndc(static_cast<float>(ys[n - 1])),
                    has_z ? z_range.to_ndc(static_cast<float>(all_z[ei][n - 1])) : 0.F,
                };
                write_vertex(out, sep_pos, color, 0.F);
                write_vertex(out, sep_pos, color, 0.F);
            }
        }
    };
}

// =============================================================================
// ScatterBuilder::done
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> ScatterBuilder::done() const
{
    auto x_mappings = m_state.x_mappings();
    auto y_mappings = m_state.y_mappings();
    auto z_mappings = m_state.z_mappings();
    const auto global_palette = m_state.palette();
    const float point_size = m_point_size;

    return [x_mappings, y_mappings, z_mappings, global_palette, point_size](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        std::vector<std::span<const double>> all_x, all_z;
        for (auto& m : x_mappings) {
            auto s = series_for_mapping(*container, m);
            all_x.insert(all_x.end(), s.begin(), s.end());
        }
        for (auto& m : z_mappings) {
            auto s = series_for_mapping(*container, m);
            all_z.insert(all_z.end(), s.begin(), s.end());
        }

        AxisRange x_range = merge_axis(x_mappings);
        AxisRange z_range = z_mappings.empty() ? AxisRange {} : merge_axis(z_mappings);
        apply_auto_scale(x_range, all_x);
        apply_auto_scale(z_range, all_z);

        out.clear();

        size_t global_idx = 0;
        for (auto& y_mapping : y_mappings) {
            auto y_series = series_for_mapping(*container, y_mapping);
            auto y_range = y_mapping.range;
            apply_auto_scale(y_range, y_series);

            for (size_t si = 0; si < y_series.size(); ++si, ++global_idx) {
                const auto& ys = y_series[si];
                const size_t n = ys.size();
                if (n == 0)
                    continue;

                const glm::vec3 color = resolve_color(y_mapping, global_palette, si);
                const size_t x_idx = std::min(global_idx, all_x.empty() ? size_t(0) : all_x.size() - 1);
                const bool has_x = !all_x.empty() && all_x[x_idx].size() == n;
                const bool has_z = global_idx < all_z.size() && all_z[global_idx].size() == n;

                for (size_t i = 0; i < n; ++i) {
                    const float px = has_x
                        ? x_range.to_ndc(static_cast<float>(all_x[x_idx][i]))
                        : x_range.to_ndc(x_range.min
                              + static_cast<float>(i) / static_cast<float>(std::max<size_t>(n - 1, 1))
                                  * (x_range.max - x_range.min));

                    write_vertex(out,
                        { px,
                            y_range.to_ndc(static_cast<float>(ys[i])),
                            has_z ? z_range.to_ndc(static_cast<float>(all_z[global_idx][i])) : 0.F },
                        color,
                        point_size);
                }
            }
        }
    };
}

// =============================================================================
// BarsBuilder::done
// =============================================================================

GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> BarsBuilder::done() const
{
    auto x_mappings = m_state.x_mappings();
    auto y_mappings = m_state.y_mappings();
    const auto global_palette = m_state.palette();

    return [x_mappings, y_mappings, global_palette](
               const std::shared_ptr<Kakshya::PlotContainer>& container,
               std::vector<uint8_t>& out,
               Element&) mutable {
        if (!container)
            return;

        container->process_default();

        AxisRange x_range = x_mappings.empty() ? AxisRange {} : merge_axis(x_mappings);
        out.clear();

        for (auto& y_mapping : y_mappings) {
            auto y_series = series_for_mapping(*container, y_mapping);
            if (y_series.empty())
                continue;

            auto y_range = y_mapping.range;
            apply_auto_scale(y_range, y_series);

            for (size_t si = 0; si < y_series.size(); ++si) {
                const auto& series = y_series[si];
                const size_t n = series.size();
                if (n == 0)
                    continue;

                const glm::vec3 color = resolve_color(y_mapping, global_palette, si);
                const float bar_w = (x_range.max - x_range.min) / static_cast<float>(n);
                const float y_base = y_range.to_ndc(0.F);

                for (size_t i = 0; i < n; ++i) {
                    const float x_left = x_range.to_ndc(x_range.min + static_cast<float>(i) * bar_w);
                    const float x_right = x_range.to_ndc(x_range.min + static_cast<float>(i + 1) * bar_w);
                    const float y_top = y_range.to_ndc(static_cast<float>(series[i]));

                    write_vertex(out, { x_left, y_base, 0.F }, color, 0.F);
                    write_vertex(out, { x_right, y_base, 0.F }, color, 0.F);
                    write_vertex(out, { x_left, y_top, 0.F }, color, 0.F);
                    write_vertex(out, { x_right, y_base, 0.F }, color, 0.F);
                    write_vertex(out, { x_right, y_top, 0.F }, color, 0.F);
                    write_vertex(out, { x_left, y_top, 0.F }, color, 0.F);
                }
            }
        }
    };
}

} // namespace MayaFlux::Portal::Forma::Plot
