#pragma once

#include "AxisRange.hpp"

#include "MayaFlux/Kakshya/NDData/NDData.hpp"
#include "MayaFlux/Portal/Forma/Primitives/Mapped.hpp"

namespace MayaFlux::Kakshya {
class PlotContainer;
}

namespace MayaFlux::Portal::Forma::Plot {

class WaveformBuilder;
class ScatterBuilder;
class BarsBuilder;

// =============================================================================
// SeriesBuilder
// =============================================================================

/**
 * @struct SeriesSpec
 * @brief Result of a SeriesBuilder terminal. Bundles a geometry function
 *        with the topology and capacity arithmetic implied by the encoding.
 *
 * Passed to Plot::place() so callers never supply topology or buffer
 * capacity manually. The raw GeometryFn<shared_ptr<PlotContainer>> is
 * still accessible via .fn for callers that need it directly.
 */
struct SeriesSpec {
    Forma::GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> fn;
    Graphics::PrimitiveTopology topology;
    std::function<size_t(uint64_t sample_count)> capacity_for;
};

/**
 * @class SeriesBuilder
 * @brief Convenience constructor for GeometryFn<shared_ptr<PlotContainer>>.
 *
 * Accumulates axis role mappings and palette, then produces a geometry
 * function via an encoding terminal (.as_waveform(), .as_scatter(), .as_bars()).
 *
 * This is an illustrative convenience surface over the GeometryFn contract,
 * not the primary or idiomatic path. The raw lambda is always preferred when
 * the builder cannot express what you need:
 *
 * @code
 * // raw — full power, always valid
 * GeometryFn<std::shared_ptr<Kakshya::PlotContainer>> geom =
 *     [](auto c, auto& out, auto& el) { ... };
 *
 * // builder — convenience for common cases
 * auto geom = Plot::series()
 *     .y(Role::SPATIAL_Y, AxisRange{}.auto_scale(), { 0.2F, 0.8F, 1.0F })
 *     .color({ 0.8F, 0.4F, 0.1F })   // fallback for unmapped series
 *     .as_waveform()
 *     .done();
 * @endcode
 *
 * Multiple .x(), .y(), .z() calls accumulate independently. Each call appends
 * a role-to-axis mapping with its own optional palette. The full PlotContainer
 * is always passed to the generated function — no data is filtered or intercepted.
 *
 * Color resolution order at geometry time:
 *   1. Per-mapping palette (cycled across series within that mapping).
 *   2. Global palette set via .color() (cycled across all series).
 *   3. White.
 *
 * When the builder cannot express what you need, write the raw lambda.
 * It receives the same PlotContainer.
 */
class SeriesBuilder {
public:
    using Role = Kakshya::DataDimension::Role;

    /**
     * @struct AxisMapping
     * @brief One or more roles mapped to a shared AxisRange and optional palette.
     *
     * palette cycles across series within this mapping. When empty, the
     * global SeriesBuilder palette is used as fallback.
     */
    struct AxisMapping {
        std::vector<Role> roles;
        AxisRange range;
        std::vector<glm::vec3> palette;
    };

    // =========================================================================
    // Axis configuration — all additive, multiple calls accumulate
    // =========================================================================

    /**
     * @brief Map a single role to the X axis.
     */
    SeriesBuilder& x(Role role, AxisRange range = {})
    {
        m_x.push_back({ .roles = { role }, .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map a single role to the X axis with per-mapping colors.
     */
    SeriesBuilder& x(Role role, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_x.push_back({ .roles = { role }, .range = std::move(range), .palette = palette });
        return *this;
    }

    /**
     * @brief Map multiple roles to the X axis with a shared range.
     */
    SeriesBuilder& x(std::vector<Role> roles, AxisRange range = {})
    {
        m_x.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map multiple roles to the X axis with a shared range and per-mapping colors.
     */
    SeriesBuilder& x(std::vector<Role> roles, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_x.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = palette });
        return *this;
    }

    /**
     * @brief Map a single role to the Y axis.
     */
    SeriesBuilder& y(Role role, AxisRange range = {})
    {
        m_y.push_back({ .roles = { role }, .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map a single role to the Y axis with per-mapping colors.
     */
    SeriesBuilder& y(Role role, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_y.push_back({ .roles = { role }, .range = std::move(range), .palette = palette });
        return *this;
    }

    /**
     * @brief Map multiple roles to the Y axis with a shared range.
     */
    SeriesBuilder& y(std::vector<Role> roles, AxisRange range = {})
    {
        m_y.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map multiple roles to the Y axis with a shared range and per-mapping colors.
     */
    SeriesBuilder& y(std::vector<Role> roles, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_y.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = palette });
        return *this;
    }

    /**
     * @brief Map a single role to the Z axis.
     */
    SeriesBuilder& z(Role role, AxisRange range = {})
    {
        m_z.push_back({ .roles = { role }, .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map a single role to the Z axis with per-mapping colors.
     */
    SeriesBuilder& z(Role role, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_z.push_back({ .roles = { role }, .range = std::move(range), .palette = palette });
        return *this;
    }

    /**
     * @brief Map multiple roles to the Z axis with a shared range.
     */
    SeriesBuilder& z(std::vector<Role> roles, AxisRange range = {})
    {
        m_z.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = {} });
        return *this;
    }

    /**
     * @brief Map multiple roles to the Z axis with a shared range and per-mapping colors.
     */
    SeriesBuilder& z(std::vector<Role> roles, AxisRange range, std::initializer_list<glm::vec3> palette)
    {
        m_z.push_back({ .roles = std::move(roles), .range = std::move(range), .palette = palette });
        return *this;
    }

    // =========================================================================
    // Global palette — fallback when a mapping carries no per-mapping colors
    // =========================================================================

    /**
     * @brief Append colors to the global palette.
     *
     * Used as fallback when a series belongs to a mapping with no per-mapping
     * palette. Cycled across all such series by index. Multiple calls append.
     */
    SeriesBuilder& color(std::initializer_list<glm::vec3> colors)
    {
        m_palette.insert(m_palette.end(), colors.begin(), colors.end());
        return *this;
    }

    SeriesBuilder& color(glm::vec3 c)
    {
        m_palette.push_back(c);
        return *this;
    }

    // =========================================================================
    // Encoding terminals
    // =========================================================================

    [[nodiscard]] WaveformBuilder as_waveform() const;
    [[nodiscard]] ScatterBuilder as_scatter() const;
    [[nodiscard]] BarsBuilder as_bars() const;

    // =========================================================================
    // State accessors — used by encoding terminals in .cpp
    // =========================================================================

    [[nodiscard]] const std::vector<AxisMapping>& x_mappings() const { return m_x; }
    [[nodiscard]] const std::vector<AxisMapping>& y_mappings() const { return m_y; }
    [[nodiscard]] const std::vector<AxisMapping>& z_mappings() const { return m_z; }
    [[nodiscard]] const std::vector<glm::vec3>& palette() const { return m_palette; }

private:
    std::vector<AxisMapping> m_x;
    std::vector<AxisMapping> m_y;
    std::vector<AxisMapping> m_z;
    std::vector<glm::vec3> m_palette;
};

// =============================================================================
// Encoding builders
// =============================================================================

/**
 * @class WaveformBuilder
 * @brief Terminal builder for LINE_STRIP waveform encoding.
 *
 * Reads all Y-axis mappings as line series. X-axis mappings provide
 * explicit sample positions; when absent, positions are distributed
 * uniformly. Z-axis mappings map to vertex Z when present.
 *
 * N Y-series in the container = N LINE_STRIP runs, one per series,
 * separated by degenerate vertices, sharing one FormaBuffer.
 * Colors are resolved per series: per-mapping palette first, global
 * palette as fallback, white if both are empty.
 *
 * Roles not consumed by this encoding remain in the container and are
 * available to any other GeometryFn produced from the same SeriesBuilder.
 *
 * .done() produces a GeometryFn<shared_ptr<PlotContainer>>.
 */
class WaveformBuilder {
public:
    WaveformBuilder(SeriesBuilder state)
        : m_state(std::move(state))
    {
    }

    /** @brief Line thickness in pixels. Default 1.5. */
    WaveformBuilder& thickness(float t)
    {
        m_thickness = t;
        return *this;
    }

    [[nodiscard]] SeriesSpec done() const;

private:
    SeriesBuilder m_state;
    float m_thickness { 1.5F };
};

/**
 * @class ScatterBuilder
 * @brief Terminal builder for POINT_LIST scatter encoding.
 *
 * Pairs X and Y axis mappings into (x[i], y[i]) point series. Z mappings
 * map to vertex Z. N X/Y pairs = N point clouds in one FormaBuffer.
 * Colors resolved per series: per-mapping palette first, global fallback.
 *
 * .done() produces a GeometryFn<shared_ptr<PlotContainer>>.
 */
class ScatterBuilder {
public:
    ScatterBuilder(SeriesBuilder state)
        : m_state(std::move(state))
    {
    }

    /** @brief Point size in pixels. Default 4. */
    ScatterBuilder& point_size(float s)
    {
        m_point_size = s;
        return *this;
    }

    [[nodiscard]] SeriesSpec done() const;

private:
    SeriesBuilder m_state;
    float m_point_size { 4.F };
};

/**
 * @class BarsBuilder
 * @brief Terminal builder for TRIANGLE_LIST bar chart encoding.
 *
 * X mappings provide bar positions; absent = uniform tiling.
 * Y mappings provide bar heights. N Y-series = N bar sets.
 * Colors resolved per series: per-mapping palette first, global fallback.
 * Z is unused by this encoding.
 *
 * .done() produces a GeometryFn<shared_ptr<PlotContainer>>.
 */
class BarsBuilder {
public:
    BarsBuilder(SeriesBuilder state)
        : m_state(std::move(state))
    {
    }

    [[nodiscard]] SeriesSpec done() const;

private:
    SeriesBuilder m_state;
};

inline WaveformBuilder SeriesBuilder::as_waveform() const { return { *this }; }
inline ScatterBuilder SeriesBuilder::as_scatter() const { return { *this }; }
inline BarsBuilder SeriesBuilder::as_bars() const { return { *this }; }

// =============================================================================
// Entry point
// =============================================================================

/**
 * @brief Begin a SeriesBuilder chain.
 *
 * Convenience constructor for GeometryFn<shared_ptr<PlotContainer>>.
 * Illustrative, not idiomatic. The raw GeometryFn lambda is always the
 * primary path and is preferred when the builder cannot express what you need.
 *
 * @code
 * // 7 SPATIAL_Y series -> 7 waveforms, blue palette
 * auto geom = Plot::series()
 *     .y(Role::SPATIAL_Y, AxisRange{}.auto_scale(), { 0.2F, 0.8F, 1.0F })
 *     .as_waveform()
 *     .thickness(1.5F)
 *     .done();
 *
 * // Lissajous scatter, X and Y each with their own range
 * auto geom = Plot::series()
 *     .x(Role::SPATIAL_X, AxisRange{}.range(-1.F, 1.F))
 *     .y(Role::SPATIAL_Y, AxisRange{}.range(-1.F, 1.F), { 0.9F, 0.3F, 0.8F })
 *     .as_scatter()
 *     .point_size(3.F)
 *     .done();
 * @endcode
 */
[[nodiscard]] inline SeriesBuilder series()
{
    return SeriesBuilder {};
}

} // namespace MayaFlux::Portal::Forma::Plot
