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

/**
 * @brief Arithmetic centroid of a plain position span.
 *
 * @param  pts Non-owning span of positions.
 * @return Mean position, or the zero vector for an empty span.
 */
[[nodiscard]] inline glm::vec3 centroid(std::span<const glm::vec3> pts) noexcept
{
    if (pts.empty())
        return glm::vec3(0.0F);
    glm::vec3 acc(0.0F);
    for (const auto& p : pts)
        acc += p;
    return acc / static_cast<float>(pts.size());
}

/**
 * @brief Position of one record in interleaved vertex bytes.
 *
 * @param  bytes  Interleaved records.
 * @param  stride Bytes per record.
 * @param  offset Byte offset of the vec3 position within a record.
 * @param  index  Record index.
 * @return The position, or nullopt when the position does not lie inside
 *         @p bytes or @p stride is too small to hold it.
 */
[[nodiscard]] inline std::optional<glm::vec3> position_at(
    std::span<const std::byte> bytes,
    size_t stride,
    size_t offset,
    size_t index) noexcept
{
    if (stride < offset + sizeof(glm::vec3)
        || index * stride + offset + sizeof(glm::vec3) > bytes.size())
        return std::nullopt;
    glm::vec3 p;
    std::memcpy(&p, bytes.data() + index * stride + offset, sizeof(glm::vec3));
    return p;
}

/**
 * @brief Arithmetic centroid of the positions in interleaved vertex bytes.
 *
 * Every Kakshya vertex record is 60 bytes with the position at offset 0, but
 * any stride and offset work, so readback bytes, node vertex data and MeshData
 * all go through here without a typed copy.
 *
 * @param  bytes  Interleaved records. A trailing partial record is ignored.
 * @param  stride Bytes per record.
 * @param  offset Byte offset of the vec3 position within a record.
 * @return Mean position, or the zero vector when no whole record fits or
 *         @p stride is too small to hold a position.
 */
[[nodiscard]] inline glm::vec3 centroid(
    std::span<const std::byte> bytes,
    size_t stride,
    size_t offset = 0) noexcept
{
    if (stride < offset + sizeof(glm::vec3))
        return glm::vec3(0.0F);
    const size_t count = bytes.size() / stride;
    if (count == 0)
        return glm::vec3(0.0F);
    glm::vec3 acc(0.0F);
    for (size_t i = 0; i < count; ++i)
        acc += position_at(bytes, stride, offset, i).value_or(glm::vec3(0.0F));
    return acc / static_cast<float>(count);
}

/**
 * @brief Area-weighted centroid of the triangles of an indexed mesh.
 *
 * Each triangle contributes its own centroid weighted by its area, so dense
 * tessellation does not pull the result the way a vertex mean does. A triangle
 * with an index outside @p bytes is skipped.
 *
 * @param  bytes   Interleaved vertex records.
 * @param  stride  Bytes per record.
 * @param  offset  Byte offset of the vec3 position within a record.
 * @param  indices Triangle list, three indices per face.
 * @return Surface centroid, or the vertex centroid when there is no
 *         triangle of nonzero area.
 */
[[nodiscard]] inline glm::vec3 surface_centroid(
    std::span<const std::byte> bytes,
    size_t stride,
    size_t offset,
    std::span<const uint32_t> indices) noexcept
{
    glm::vec3 acc(0.0F);
    float total = 0.0F;
    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        const auto a = position_at(bytes, stride, offset, indices[t]);
        const auto b = position_at(bytes, stride, offset, indices[t + 1]);
        const auto c = position_at(bytes, stride, offset, indices[t + 2]);
        if (!a || !b || !c)
            continue;
        const float area = 0.5F * glm::length(glm::cross(*b - *a, *c - *a));
        acc += (*a + *b + *c) * (area / 3.0F);
        total += area;
    }
    return total > 0.0F ? acc / total : centroid(bytes, stride, offset);
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

/**
 * @brief Axis-aligned bounding box of the positions in interleaved vertex bytes.
 *
 * Counterpart of the byte centroid: any stride and offset work, so readback
 * bytes, node vertex data and MeshData go through here without a typed copy.
 *
 * @param  bytes  Interleaved records. A trailing partial record is ignored.
 * @param  stride Bytes per record.
 * @param  offset Byte offset of the vec3 position within a record.
 * @return Tightest box, or AABB3D{zero, zero} when no whole record fits or
 *         @p stride is too small to hold a position.
 */
[[nodiscard]] inline AABB3D aabb(
    std::span<const std::byte> bytes,
    size_t stride,
    size_t offset = 0) noexcept
{
    const AABB3D empty { .min = glm::vec3(0.0F), .max = glm::vec3(0.0F) };
    if (stride < offset + sizeof(glm::vec3))
        return empty;
    const size_t count = bytes.size() / stride;
    if (count == 0)
        return empty;
    constexpr float inf = std::numeric_limits<float>::max();
    AABB3D box { .min = glm::vec3(inf), .max = glm::vec3(-inf) };
    for (size_t i = 0; i < count; ++i) {
        const glm::vec3 p = position_at(bytes, stride, offset, i).value_or(glm::vec3(0.0F));
        box.min = glm::min(box.min, p);
        box.max = glm::max(box.max, p);
    }
    return box;
}

} // namespace MayaFlux::Kinesis
