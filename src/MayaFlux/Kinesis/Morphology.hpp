#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"

namespace MayaFlux::Kinesis {

// =============================================================================
// PositionCarrying concept
// =============================================================================

/**
 * @concept PositionCarrying
 * @brief Satisfied by any type exposing a @c position field convertible to glm::vec3.
 *
 * Matched by all Kakshya vertex types (Vertex, PointVertex, LineVertex,
 * MeshVertex, TextureQuadVertex), any user-defined struct following the same
 * convention, and glm::vec3 itself when wrapped in a trivial aggregate.
 *
 * Functions constrained by PositionCarrying accept arbitrary point sets
 * without specialisation: mesh vertices, point clouds, particle systems,
 * Forma geometry, CV observations, or any future vertex kind.
 */
template <typename T>
concept PositionCarrying = requires(const T& v) {
    { v.position } -> std::convertible_to<glm::vec3>;
};

// =============================================================================
// centroid
// =============================================================================

/**
 * @brief Arithmetic centroid (uniform weight) of a PositionCarrying span.
 *
 * Computes the first geometric moment with measure 1 per point.
 * Returns the zero vector for an empty span.
 *
 * @tparam T Any type satisfying PositionCarrying.
 * @param  pts Non-owning span of points.
 * @return Mean position.
 */
template <PositionCarrying T>
[[nodiscard]] glm::vec3 centroid(std::span<T> pts) noexcept
{
    if (pts.empty())
        return glm::vec3(0.0F);
    glm::vec3 acc(0.0F);
    for (const auto& p : pts)
        acc += static_cast<glm::vec3>(p.position);
    return acc / static_cast<float>(pts.size());
}

/**
 * @brief Scalar-weighted centroid of a PositionCarrying span.
 *
 * Each point contributes @p weight(point) to the measure. Suitable for
 * mesh vertices weighted by deformation magnitude, particles weighted by
 * energy (size / thickness field), or any domain-specific scalar.
 *
 * Falls back to the zero vector when total weight is zero or the span is empty.
 *
 * @tparam T        Any type satisfying PositionCarrying.
 * @tparam WeightFn Callable: @c const T& -> float (or any float-convertible type).
 * @param  pts      Non-owning span of points.
 * @param  weight   Per-point weight callable.
 * @return Weighted mean position.
 */
template <PositionCarrying T, std::invocable<const T&> WeightFn>
    requires std::convertible_to<std::invoke_result_t<WeightFn, const T&>, float>
[[nodiscard]] glm::vec3 centroid(std::span<T> pts, WeightFn weight) noexcept
{
    if (pts.empty())
        return glm::vec3(0.0F);
    glm::vec3 acc(0.0F);
    float total = 0.0F;
    for (const auto& p : pts) {
        const auto w = static_cast<float>(std::invoke(weight, p));
        acc += static_cast<glm::vec3>(p.position) * w;
        total += w;
    }
    return total > 0.0F ? acc / total : glm::vec3(0.0F);
}

/**
 * @brief Fully explicit weighted centroid for arbitrary point types.
 *
 * Neither PositionCarrying nor any field convention is required. Both the
 * position and the weight are extracted via caller-supplied callables. Use
 * this overload for Nexus QueryResult spans, CV observation structs, Eigen
 * column references, or any type that does not follow the vertex convention.
 *
 * Falls back to the zero vector when total weight is zero or the span is empty.
 *
 * @tparam T        Element type. No concept constraint.
 * @tparam PosFn    Callable: @c const T& -> glm::vec3 (or convertible).
 * @tparam WeightFn Callable: @c const T& -> float (or convertible).
 * @param  pts      Non-owning span of elements.
 * @param  pos      Position extractor.
 * @param  weight   Weight extractor.
 * @return Weighted mean position.
 */
template <typename T,
    std::invocable<const T&> PosFn,
    std::invocable<const T&> WeightFn>
    requires std::convertible_to<std::invoke_result_t<PosFn, const T&>, glm::vec3>
    && std::convertible_to<std::invoke_result_t<WeightFn, const T&>, float>
[[nodiscard]] glm::vec3 centroid(std::span<T> pts, PosFn pos, WeightFn weight) noexcept
{
    if (pts.empty())
        return glm::vec3(0.0F);
    glm::vec3 acc(0.0F);
    float total = 0.0F;
    for (const auto& p : pts) {
        const auto w = static_cast<float>(std::invoke(weight, p));
        acc += static_cast<glm::vec3>(std::invoke(pos, p)) * w;
        total += w;
    }
    return total > 0.0F ? acc / total : glm::vec3(0.0F);
}

// =============================================================================
// aabb
// =============================================================================

/**
 * @brief Axis-aligned bounding box of a PositionCarrying span.
 *
 * Returns AABB3D{zero, zero} for an empty span. No allocation; single
 * linear pass.
 *
 * @tparam T  Any type satisfying PositionCarrying.
 * @param  pts Non-owning span of points.
 * @return Tightest AABB enclosing all positions in @p pts.
 */
template <PositionCarrying T>
[[nodiscard]] AABB3D aabb(std::span<T> pts) noexcept
{
    if (pts.empty())
        return AABB3D { .min = glm::vec3(0.0F), .max = glm::vec3(0.0F) };
    constexpr float inf = std::numeric_limits<float>::max();
    AABB3D box { .min = glm::vec3(inf), .max = glm::vec3(-inf) };
    for (const auto& p : pts) {
        const glm::vec3 q = static_cast<glm::vec3>(p.position);
        box.min = glm::min(box.min, q);
        box.max = glm::max(box.max, q);
    }
    return box;
}

} // namespace MayaFlux::Kinesis
