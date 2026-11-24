#include "Incidence.hpp"

namespace MayaFlux::Kinesis {

std::optional<Incidence> Incidence::create(
    std::span<const uint32_t> face_sizes,
    std::span<const uint32_t> corners,
    size_t vertex_count)
{
    constexpr uint64_t limit = std::numeric_limits<uint32_t>::max();
    if (face_sizes.size() >= limit || corners.size() > limit || vertex_count > limit) {
        return std::nullopt;
    }

    Incidence incidence;
    incidence.m_offsets.resize(face_sizes.size() + 1);

    uint64_t running = 0;
    for (size_t face = 0; face < face_sizes.size(); ++face) {
        if (face_sizes[face] < 3) {
            return std::nullopt;
        }
        incidence.m_offsets[face] = static_cast<uint32_t>(running);
        running += face_sizes[face];
        if (running > limit) {
            return std::nullopt;
        }
    }
    incidence.m_offsets.back() = static_cast<uint32_t>(running);
    if (running != corners.size()) {
        return std::nullopt;
    }

    const auto in_range = [vertex_count](uint32_t vertex) { return vertex < vertex_count; };
    const bool valid = corners.size() >= detail::k_parallel_threshold
        ? std::all_of(std::execution::par_unseq, corners.begin(), corners.end(), in_range)
        : std::ranges::all_of(corners, in_range);
    if (!valid) {
        return std::nullopt;
    }

    incidence.m_sizes = face_sizes;
    incidence.m_corners = corners;
    incidence.m_vertex_count = vertex_count;
    return incidence;
}

std::vector<uint32_t> Incidence::corner_faces() const
{
    std::vector<uint32_t> faces(m_corners.size());
    detail::for_each_indexed(std::execution::par_unseq, m_sizes,
        [&](size_t face, const uint32_t& size) {
            std::fill_n(faces.data() + m_offsets[face], size, static_cast<uint32_t>(face));
        });
    return faces;
}

VertexCorners Incidence::vertex_corners() const
{
    VertexCorners result;
    result.offsets.assign(m_vertex_count + 1, 0U);
    for (const uint32_t vertex : m_corners) {
        ++result.offsets[static_cast<size_t>(vertex) + 1];
    }
    std::partial_sum(result.offsets.begin(), result.offsets.end(), result.offsets.begin());

    result.corners.resize(m_corners.size());
    std::vector<uint32_t> cursor(result.offsets.begin(), result.offsets.end() - 1);
    for (uint32_t corner = 0; corner < m_corners.size(); ++corner) {
        result.corners[cursor[m_corners[corner]]++] = corner;
    }
    return result;
}

} // namespace MayaFlux::Kinesis
