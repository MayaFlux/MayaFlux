#pragma once

#include "Stochastic.hpp"

#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"

namespace MayaFlux::Kinesis {

using Vertex = Kakshya::Vertex;

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
[[nodiscard]] MAYAFLUX_API std::vector<Vertex> generate_samples(
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
[[nodiscard]] MAYAFLUX_API Vertex generate_sample_at(
    SpatialDistribution distribution,
    size_t index,
    size_t total,
    const SamplerBounds& bounds,
    Kinesis::Stochastic::Stochastic& rng);

} // namespace MayaFlux::Kinesis
