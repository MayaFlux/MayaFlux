#include "Polygon.hpp"

#include <Eigen/Core>

namespace MayaFlux::Kinesis {

namespace {

    using glm::dvec3;

    dvec3 point_at(std::span<const glm::vec3> positions, uint32_t index) noexcept
    {
        return dvec3(positions[index]);
    }

    dvec3 area_vector(std::span<const glm::vec3> positions, std::span<const uint32_t> ring) noexcept
    {
        const size_t count = ring.size();
        if (count < 3) {
            return dvec3(0.0);
        }

        const dvec3 origin = point_at(positions, ring[0]);
        dvec3 sum(0.0);
        dvec3 previous = point_at(positions, ring[1]) - origin;
        for (size_t i = 2; i < count; ++i) {
            const dvec3 next = point_at(positions, ring[i]) - origin;
            sum += glm::cross(previous, next);
            previous = next;
        }
        return 0.5 * sum;
    }

    std::optional<dvec3> unit_normal(const dvec3& area) noexcept
    {
        const double length = glm::length(area);
        if (!(length > 0.0) || !std::isfinite(length)) {
            return std::nullopt;
        }
        return area / length;
    }

    dvec3 vertex_mean(std::span<const glm::vec3> positions, std::span<const uint32_t> ring) noexcept
    {
        dvec3 sum(0.0);
        for (const uint32_t index : ring) {
            sum += point_at(positions, index);
        }
        return ring.empty() ? sum : sum / static_cast<double>(ring.size());
    }

    dvec3 enclosed_centroid(
        std::span<const glm::vec3> positions, std::span<const uint32_t> ring, const dvec3& normal) noexcept
    {
        const dvec3 origin = point_at(positions, ring[0]);
        dvec3 weighted(0.0);
        double total = 0.0;
        dvec3 previous = point_at(positions, ring[1]) - origin;
        for (size_t i = 2; i < ring.size(); ++i) {
            const dvec3 next = point_at(positions, ring[i]) - origin;
            const double weight = 0.5 * glm::dot(glm::cross(previous, next), normal);
            weighted += weight * (previous + next) / 3.0;
            total += weight;
            previous = next;
        }
        if (!(std::abs(total) > 0.0)) {
            return vertex_mean(positions, ring);
        }
        return origin + weighted / total;
    }

} // namespace

namespace detail {

    void normalize_in_place(std::span<glm::vec3> vectors)
    {
        static_assert(sizeof(glm::vec3) == 3 * sizeof(float));
        if (vectors.empty()) {
            return;
        }

        using Row = Eigen::Array<float, 1, Eigen::Dynamic>;
        Eigen::Map<Eigen::Matrix<float, 3, Eigen::Dynamic>> columns(
            reinterpret_cast<float*>(vectors.data()), 3, static_cast<Eigen::Index>(vectors.size()));

        const Row norms = columns.colwise().norm().array();
        const Row divisor = (norms > 0.0F).select(norms, Row::Ones(norms.size()));
        columns.array() /= divisor.replicate<3, 1>();
    }

} // namespace detail

glm::vec3 polygon_area_vector(std::span<const glm::vec3> positions, std::span<const uint32_t> ring)
{
    return glm::vec3(area_vector(positions, ring));
}

float polygon_area(std::span<const glm::vec3> positions, std::span<const uint32_t> ring)
{
    return static_cast<float>(glm::length(area_vector(positions, ring)));
}

std::optional<glm::vec3> polygon_normal(std::span<const glm::vec3> positions, std::span<const uint32_t> ring)
{
    const auto normal = unit_normal(area_vector(positions, ring));
    if (!normal) {
        return std::nullopt;
    }
    return glm::vec3(*normal);
}

glm::vec3 polygon_centroid(std::span<const glm::vec3> positions, std::span<const uint32_t> ring)
{
    if (ring.size() < 3) {
        return glm::vec3(vertex_mean(positions, ring));
    }
    const auto normal = unit_normal(area_vector(positions, ring));
    if (!normal) {
        return glm::vec3(vertex_mean(positions, ring));
    }
    return glm::vec3(enclosed_centroid(positions, ring, *normal));
}

std::optional<float> polygon_planarity(std::span<const glm::vec3> positions, std::span<const uint32_t> ring)
{
    if (ring.size() < 3) {
        return std::nullopt;
    }
    const auto normal = unit_normal(area_vector(positions, ring));
    if (!normal) {
        return std::nullopt;
    }
    if (ring.size() == 3) {
        return 0.0F;
    }

    const dvec3 centroid = enclosed_centroid(positions, ring, *normal);
    double farthest = 0.0;
    for (const uint32_t index : ring) {
        farthest = std::max(farthest, std::abs(glm::dot(point_at(positions, index) - centroid, *normal)));
    }
    return static_cast<float>(farthest);
}

bool polygon_is_convex(std::span<const glm::vec3> positions, std::span<const uint32_t> ring, float tolerance)
{
    const size_t count = ring.size();
    if (count < 3) {
        return false;
    }
    const auto normal = unit_normal(area_vector(positions, ring));
    if (!normal) {
        return false;
    }
    if (count == 3) {
        return true;
    }

    const double slack = std::max(static_cast<double>(tolerance), 0.0);
    double turning = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const dvec3 a = point_at(positions, ring[i]);
        const dvec3 b = point_at(positions, ring[(i + 1) % count]);
        const dvec3 c = point_at(positions, ring[(i + 2) % count]);
        const dvec3 first = b - a;
        const dvec3 second = c - b;
        const double scale = glm::length(first) * glm::length(second);
        if (!(scale > 0.0)) {
            continue;
        }

        const double turn = glm::dot(glm::cross(first, second), *normal);
        if (turn < -slack * scale) {
            return false;
        }
        if (count >= 5) {
            turning += std::atan2(turn, glm::dot(first, second));
        }
    }

    if (count >= 5) {
        constexpr double full_turn = 2.0 * std::numbers::pi;
        return std::abs(turning - full_turn) < 1.0e-4;
    }
    return true;
}

std::optional<std::vector<glm::vec3>> face_area_vectors(
    std::span<const glm::vec3> positions, const Incidence& incidence)
{
    if (positions.size() < incidence.vertex_count()) {
        return std::nullopt;
    }

    std::vector<glm::vec3> vectors(incidence.face_count());
    detail::for_each_indexed(std::execution::par_unseq, std::span<glm::vec3>(vectors),
        [&](size_t face, glm::vec3& out) {
            out = glm::vec3(area_vector(positions, incidence.face(face)));
        });
    return vectors;
}

std::optional<std::vector<glm::vec3>> face_normals(
    std::span<const glm::vec3> positions, const Incidence& incidence)
{
    auto vectors = face_area_vectors(positions, incidence);
    if (vectors) {
        detail::normalize_in_place(*vectors);
    }
    return vectors;
}

std::optional<std::vector<float>> face_areas(
    std::span<const glm::vec3> positions, const Incidence& incidence)
{
    if (positions.size() < incidence.vertex_count()) {
        return std::nullopt;
    }

    std::vector<float> areas(incidence.face_count());
    detail::for_each_indexed(std::execution::par_unseq, std::span<float>(areas),
        [&](size_t face, float& out) {
            out = static_cast<float>(glm::length(area_vector(positions, incidence.face(face))));
        });
    return areas;
}

} // namespace MayaFlux::Kinesis
