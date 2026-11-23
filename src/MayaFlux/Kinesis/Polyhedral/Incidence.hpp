#pragma once

#include <execution>

namespace MayaFlux::Kinesis {

/**
 * @enum AttributeSupport
 * @brief Where an attribute of a polyhedral surface is defined.
 */
enum class AttributeSupport : uint8_t {
    Constant, ///< One value for the whole surface.
    Face, ///< One value per face.
    Vertex, ///< One value per vertex.
    Corner ///< One value per corner, the slot a face holds for one of its vertices.
};

namespace detail {

    inline constexpr size_t k_parallel_threshold = 16384;

    /**
     * @brief Call @p fn(index, item) for every item, in parallel above a size threshold.
     *
     * The index is recovered from the item's address, so no index array is built.
     * @p fn must be safe to run unsequenced when @p policy is par_unseq.
     */
    template <typename Policy, typename T, typename Fn>
    void for_each_indexed(const Policy& policy, std::span<T> items, const Fn& fn)
    {
        if (items.size() < k_parallel_threshold) {
            for (size_t i = 0; i < items.size(); ++i) {
                fn(i, items[i]);
            }
            return;
        }
        T* const base = items.data();
        std::for_each(policy, items.begin(), items.end(),
            [&fn, base](T& item) { fn(static_cast<size_t>(&item - base), item); });
    }

} // namespace detail

/**
 * @struct FaceList
 * @brief The owning form of a face set: one size per face and the vertex of every corner.
 */
struct FaceList {
    std::vector<uint32_t> sizes;
    std::vector<uint32_t> corners;
};

/**
 * @struct VertexCorners
 * @brief For every vertex, the corners that name it, as a compressed row layout.
 *
 * The corners around one vertex are in ascending order.
 */
struct VertexCorners {
    std::vector<uint32_t> offsets;
    std::vector<uint32_t> corners;

    /** @brief The corners that name @p vertex. */
    [[nodiscard]] std::span<const uint32_t> around(size_t vertex) const noexcept
    {
        return std::span<const uint32_t>(corners).subspan(offsets[vertex], offsets[vertex + 1] - offsets[vertex]);
    }
};

/**
 * @class Incidence
 * @brief The face to vertex incidence of a polyhedral surface.
 *
 * A face is a cycle of at least three corners, and a corner is a slot that names one
 * vertex. Corners are numbered consecutively, face after face, so the corners of face f
 * are the half open range [offsets[f], offsets[f + 1]). The same vertex may be named by
 * any number of corners, which is how attributes that differ between faces at a shared
 * point are held.
 *
 * Only the offsets are owned. The face sizes and the corner array are viewed, so the
 * memory they live in must outlive the incidence.
 */
class MAYAFLUX_API Incidence {
public:
    /**
     * @brief Check and wrap a face set.
     * @param face_sizes Corners of each face, each at least three.
     * @param corners Vertex of every corner, face after face.
     * @param vertex_count Number of vertices the corners may name.
     * @return nullopt for a face of fewer than three corners, a size sum that is not the
     * corner count, a vertex at or past @p vertex_count, or counts beyond 32 bits.
     */
    [[nodiscard]] static std::optional<Incidence> create(
        std::span<const uint32_t> face_sizes,
        std::span<const uint32_t> corners,
        size_t vertex_count);

    [[nodiscard]] static std::optional<Incidence> create(const FaceList& faces, size_t vertex_count)
    {
        return create(faces.sizes, faces.corners, vertex_count);
    }

    [[nodiscard]] static std::optional<Incidence> create(FaceList&&, size_t) = delete;

    [[nodiscard]] size_t face_count() const noexcept { return m_sizes.size(); }
    [[nodiscard]] size_t corner_count() const noexcept { return m_corners.size(); }
    [[nodiscard]] size_t vertex_count() const noexcept { return m_vertex_count; }

    /** @brief The vertices of face @p index, in order. */
    [[nodiscard]] std::span<const uint32_t> face(size_t index) const noexcept
    {
        return m_corners.subspan(m_offsets[index], m_sizes[index]);
    }

    [[nodiscard]] std::span<const uint32_t> sizes() const noexcept { return m_sizes; }
    [[nodiscard]] std::span<const uint32_t> corners() const noexcept { return m_corners; }

    /** @brief First corner of every face, then the corner count, so one more than the faces. */
    [[nodiscard]] std::span<const uint32_t> offsets() const noexcept { return m_offsets; }

    /** @brief The face that holds each corner. */
    [[nodiscard]] std::vector<uint32_t> corner_faces() const;

    /** @brief The corners around each vertex. A vertex no face names has none. */
    [[nodiscard]] VertexCorners vertex_corners() const;

    /**
     * @brief Carry an attribute defined on @p support to one value per corner.
     * @return nullopt unless @p values has one value for Constant, one per face for Face,
     * one per vertex for Vertex, or one per corner for Corner.
     */
    template <typename T>
        requires std::default_initializable<T> && std::copyable<T>
    [[nodiscard]] std::optional<std::vector<T>> expand_to_corners(
        std::span<const T> values, AttributeSupport support) const
    {
        std::vector<T> out(m_corners.size());
        switch (support) {
        case AttributeSupport::Constant:
            if (values.size() != 1) {
                return std::nullopt;
            }
            std::ranges::fill(out, values.front());
            break;
        case AttributeSupport::Face:
            if (values.size() != m_sizes.size()) {
                return std::nullopt;
            }
            detail::for_each_indexed(std::execution::par_unseq, m_sizes,
                [&](size_t face, const uint32_t& size) {
                    std::fill_n(out.data() + m_offsets[face], size, values[face]);
                });
            break;
        case AttributeSupport::Vertex:
            if (values.size() != m_vertex_count) {
                return std::nullopt;
            }
            detail::for_each_indexed(std::execution::par_unseq, m_corners,
                [&](size_t corner, const uint32_t& vertex) { out[corner] = values[vertex]; });
            break;
        case AttributeSupport::Corner:
            if (values.size() != m_corners.size()) {
                return std::nullopt;
            }
            std::ranges::copy(values, out.begin());
            break;
        }
        return out;
    }

private:
    Incidence() = default;

    std::span<const uint32_t> m_sizes;
    std::span<const uint32_t> m_corners;
    std::vector<uint32_t> m_offsets;
    size_t m_vertex_count {};
};

} // namespace MayaFlux::Kinesis
