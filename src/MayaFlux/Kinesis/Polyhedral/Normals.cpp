#include "Normals.hpp"

#include "Polygon.hpp"

namespace MayaFlux::Kinesis {

namespace {

    double corner_angle(
        std::span<const glm::vec3> positions, std::span<const uint32_t> ring, size_t slot) noexcept
    {
        const size_t count = ring.size();
        const glm::dvec3 here(positions[ring[slot]]);
        const glm::dvec3 before = glm::dvec3(positions[ring[(slot + count - 1) % count]]) - here;
        const glm::dvec3 after = glm::dvec3(positions[ring[(slot + 1) % count]]) - here;
        return std::atan2(glm::length(glm::cross(before, after)), glm::dot(before, after));
    }

} // namespace

std::optional<std::vector<glm::vec3>> weighted_vertex_normals(
    std::span<const glm::vec3> positions,
    const Incidence& incidence,
    NormalWeighting weighting)
{
    auto face_direction = face_area_vectors(positions, incidence);
    if (!face_direction) {
        return std::nullopt;
    }
    if (weighting != NormalWeighting::Area) {
        detail::normalize_in_place(*face_direction);
    }

    const VertexCorners around = incidence.vertex_corners();
    const std::vector<uint32_t> face_of = incidence.corner_faces();
    const auto offsets = incidence.offsets();

    std::vector<glm::vec3> normals(incidence.vertex_count());
    detail::for_each_indexed(std::execution::par_unseq, std::span<glm::vec3>(normals),
        [&](size_t vertex, glm::vec3& normal) {
            glm::dvec3 sum(0.0);
            for (const uint32_t corner : around.around(vertex)) {
                const uint32_t face = face_of[corner];
                const glm::dvec3 direction((*face_direction)[face]);
                if (weighting == NormalWeighting::Angle) {
                    sum += direction * corner_angle(positions, incidence.face(face), corner - offsets[face]);
                } else {
                    sum += direction;
                }
            }
            normal = glm::vec3(sum);
        });

    detail::normalize_in_place(normals);
    return normals;
}

} // namespace MayaFlux::Kinesis
