#pragma once

#include "Stochastic.hpp"

#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

namespace MayaFlux::Kinesis {

/**
 * @brief Spatial distribution mode for point cloud and particle generation.
 *
 * Shared enumeration consumed by both ParticleNetwork and PointCloudNetwork.
 * Separates the concern of spatial distribution from vertex type construction.
 */
enum class SpatialDistribution : uint8_t {
    RANDOM_VOLUME,
    RANDOM_SURFACE,
    GRID,
    SPHERE_VOLUME,
    SPHERE_SURFACE,
    UNIFORM_GRID,
    RANDOM_SPHERE,
    RANDOM_CUBE,
    PERLIN_FIELD,
    BROWNIAN_PATH,
    STRATIFIED_CUBE,
    SPLINE_PATH,
    LISSAJOUS,
    FIBONACCI_SPHERE,
    FIBONACCI_SPIRAL,
    TORUS,
    EMPTY
};

/**
 * @struct SamplerBounds
 * @brief Spatial domain for vertex generation.
 */
struct SamplerBounds {
    glm::vec3 min { -1.0F };
    glm::vec3 max { 1.0F };

    [[nodiscard]] glm::vec3 center() const noexcept { return (min + max) * 0.5F; }
    [[nodiscard]] glm::vec3 extent() const noexcept { return max - min; }
    [[nodiscard]] float max_radius() const noexcept { return glm::length(extent()) * 0.5F; }
};

/**
 * @struct SampleResult
 * @brief Position and normalised color derived from spatial sampling.
 *
 * Color is a spatially-derived hint (normalized position, spherical angle, etc.)
 * and may be overridden by the caller. No vertex-type-specific fields are present.
 */
struct SampleResult {
    glm::vec3 position;
    glm::vec3 color { 1.0F };
    float scalar { 1.0F }; ///< Normalised scalar; maps to size (PointVertex) or thickness (LineVertex)
};

/**
 * @brief Generate a batch of spatially distributed samples.
 * @param distribution Spatial distribution algorithm to apply
 * @param count Number of samples to generate
 * @param bounds Spatial domain
 * @param rng Stochastic engine (caller owns; enables reproducible sequences)
 * @return SampleResult vector of length <= count (exact for all deterministic modes)
 *
 * All geometry is computed here. Callers convert SampleResult to their
 * concrete vertex type via the projection helpers below.
 */
[[nodiscard]] std::vector<SampleResult> generate_samples(
    SpatialDistribution distribution,
    size_t count,
    const SamplerBounds& bounds,
    Kinesis::Stochastic::Stochastic& rng);

/**
 * @brief Generate a single sample at a specific index (for indexed/sequential modes).
 * @param distribution Spatial distribution algorithm
 * @param index Sample index within the total sequence
 * @param total Total number of samples in the sequence
 * @param bounds Spatial domain
 * @param rng Stochastic engine
 * @return Single SampleResult
 *
 * Useful for ParticleNetwork's per-index generation pattern.
 */
[[nodiscard]] SampleResult generate_sample_at(
    SpatialDistribution distribution,
    size_t index,
    size_t total,
    const SamplerBounds& bounds,
    Kinesis::Stochastic::Stochastic& rng);

//-----------------------------------------------------------------------------
// Vertex projection — convert SampleResult to concrete vertex types.
// These are the ONLY places that touch PointVertex / LineVertex fields.
// Adding a new vertex type means adding one function here, nothing else.
//-----------------------------------------------------------------------------

/**
 * @brief Project SampleResult to PointVertex.
 * @param s Source sample
 * @param size_range Min/max point size range; scalar linearly interpolates within it
 * @return PointVertex with position, color, and size derived from sample
 */
[[nodiscard]] inline Nodes::PointVertex to_point_vertex(
    const SampleResult& s,
    glm::vec2 size_range = { 8.0F, 12.0F }) noexcept
{
    return {
        .position = s.position,
        .color = s.color,
        .size = glm::mix(size_range.x, size_range.y, s.scalar)
    };
}

/**
 * @brief Project SampleResult to LineVertex.
 * @param s Source sample
 * @param thickness_range Min/max line thickness range; scalar linearly interpolates within it
 * @return LineVertex with position, color, and thickness derived from sample
 */
[[nodiscard]] inline Nodes::LineVertex to_line_vertex(
    const SampleResult& s,
    glm::vec2 thickness_range = { 1.0F, 2.0F }) noexcept
{
    return {
        .position = s.position,
        .color = s.color,
        .thickness = glm::mix(thickness_range.x, thickness_range.y, s.scalar)
    };
}

/**
 * @brief Batch-project SampleResult vector to PointVertex.
 * @param samples Source samples
 * @param size_range Size range passed to to_point_vertex
 * @return PointVertex vector of equal length
 */
[[nodiscard]] std::vector<Nodes::PointVertex> to_point_vertices(
    std::span<const SampleResult> samples,
    glm::vec2 size_range = { 8.0F, 12.0F });

/**
 * @brief Batch-project SampleResult vector to LineVertex.
 * @param samples Source samples
 * @param thickness_range Thickness range passed to to_line_vertex
 * @return LineVertex vector of equal length
 */
[[nodiscard]] std::vector<Nodes::LineVertex> to_line_vertices(
    std::span<const SampleResult> samples,
    glm::vec2 thickness_range = { 1.0F, 2.0F });

} // namespace MayaFlux::Kinesis
