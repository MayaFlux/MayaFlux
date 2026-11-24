#include "Triangulation.hpp"

#include "Polygon.hpp"

#include "MayaFlux/Kinesis/Geometry2D.hpp"

namespace MayaFlux::Kinesis {

namespace {

    using glm::dvec3;

    void emit(uint32_t* out, uint32_t a, uint32_t b, uint32_t c) noexcept
    {
        out[0] = a;
        out[1] = b;
        out[2] = c;
    }

    bool split_quad(
        std::span<const glm::vec3> positions,
        std::span<const uint32_t> ring,
        bool shorter_diagonal,
        uint32_t* out) noexcept
    {
        bool second = false;
        if (shorter_diagonal) {
            const dvec3 p0(positions[ring[0]]);
            const dvec3 p1(positions[ring[1]]);
            const dvec3 p2(positions[ring[2]]);
            const dvec3 p3(positions[ring[3]]);
            const dvec3 d02 = p2 - p0;
            const dvec3 d13 = p3 - p1;
            second = glm::dot(d13, d13) < glm::dot(d02, d02);
        }

        if (second) {
            emit(out, ring[1], ring[2], ring[3]);
            emit(out + 3, ring[1], ring[3], ring[0]);
        } else {
            emit(out, ring[0], ring[1], ring[2]);
            emit(out + 3, ring[0], ring[2], ring[3]);
        }
        return true;
    }

    bool clip_concave(
        std::span<const glm::vec3> positions,
        std::span<const uint32_t> ring,
        uint32_t* out)
    {
        const size_t count = ring.size();
        dvec3 sum(0.0);
        const dvec3 origin(positions[ring[0]]);
        dvec3 previous = dvec3(positions[ring[1]]) - origin;
        for (size_t i = 2; i < count; ++i) {
            const dvec3 next = dvec3(positions[ring[i]]) - origin;
            sum += glm::cross(previous, next);
            previous = next;
        }

        const double length = glm::length(sum);
        if (!(length > 0.0) || !std::isfinite(length)) {
            return false;
        }

        const dvec3 axis = sum / length;
        const dvec3 helper = std::abs(axis.x) < 0.9 ? dvec3(1.0, 0.0, 0.0) : dvec3(0.0, 1.0, 0.0);
        const dvec3 u = glm::normalize(glm::cross(helper, axis));
        const dvec3 v = glm::cross(axis, u);

        dvec3 mean(0.0);
        for (const uint32_t index : ring) {
            mean += dvec3(positions[index]);
        }
        mean /= static_cast<double>(count);

        std::vector<glm::dvec2> planar(count);
        double extent = 0.0;
        for (size_t i = 0; i < count; ++i) {
            const dvec3 offset = dvec3(positions[ring[i]]) - mean;
            planar[i] = glm::dvec2(glm::dot(offset, u), glm::dot(offset, v));
            extent = std::max({ extent, std::abs(planar[i].x), std::abs(planar[i].y) });
        }
        if (!(extent > 0.0)) {
            return false;
        }

        std::vector<glm::vec2> scaled(count);
        std::ranges::transform(planar, scaled.begin(), [extent](const glm::dvec2& p) {
            return glm::vec2(p / extent);
        });

        const auto triangles = triangulate_polygon(scaled);
        if (triangles.size() != count - 2) {
            return false;
        }
        for (size_t t = 0; t < triangles.size(); ++t) {
            emit(out + 3 * t, ring[triangles[t][0]], ring[triangles[t][1]], ring[triangles[t][2]]);
        }
        return true;
    }

    bool triangulate_face(
        std::span<const glm::vec3> positions,
        std::span<const uint32_t> ring,
        bool shorter_diagonal,
        float convexity_tolerance,
        uint32_t* out)
    {
        const size_t count = ring.size();
        if (count == 3) {
            emit(out, ring[0], ring[1], ring[2]);
            return true;
        }

        if (polygon_is_convex(positions, ring, convexity_tolerance)) {
            if (count == 4) {
                return split_quad(positions, ring, shorter_diagonal, out);
            }
            for (size_t i = 1; i + 1 < count; ++i) {
                emit(out + 3 * (i - 1), ring[0], ring[i], ring[i + 1]);
            }
            return true;
        }

        return clip_concave(positions, ring, out);
    }

} // namespace

std::optional<Triangulation> triangulate_faces(
    std::span<const glm::vec3> positions,
    const Incidence& incidence,
    bool shorter_quad_diagonal,
    float convexity_tolerance)
{
    if (positions.size() < incidence.vertex_count()) {
        return std::nullopt;
    }

    const size_t faces = incidence.face_count();
    std::vector<size_t> first_triangle(faces + 1, 0);
    for (size_t face = 0; face < faces; ++face) {
        first_triangle[face + 1] = first_triangle[face] + (incidence.sizes()[face] - 2);
    }
    const size_t triangles = first_triangle.back();

    Triangulation result;
    result.indices.resize(triangles * 3);
    result.triangle_face.resize(triangles);

    std::vector<uint8_t> succeeded(faces, 1);
    detail::for_each_indexed(std::execution::par, std::span<uint8_t>(succeeded),
        [&](size_t face, uint8_t& ok) {
            const auto ring = incidence.face(face);
            ok = triangulate_face(positions, ring, shorter_quad_diagonal, convexity_tolerance,
                     result.indices.data() + 3 * first_triangle[face])
                ? 1
                : 0;
            std::fill_n(result.triangle_face.data() + first_triangle[face], ring.size() - 2, static_cast<uint32_t>(face));
        });

    if (std::ranges::all_of(succeeded, [](uint8_t ok) { return ok != 0; })) {
        return result;
    }

    Triangulation kept;
    kept.indices.reserve(result.indices.size());
    kept.triangle_face.reserve(result.triangle_face.size());
    for (size_t face = 0; face < faces; ++face) {
        if (succeeded[face] == 0) {
            kept.failed_faces.push_back(static_cast<uint32_t>(face));
            continue;
        }
        const size_t begin = first_triangle[face];
        const size_t end = first_triangle[face + 1];
        kept.indices.insert(kept.indices.end(),
            result.indices.begin() + static_cast<std::ptrdiff_t>(3 * begin),
            result.indices.begin() + static_cast<std::ptrdiff_t>(3 * end));
        kept.triangle_face.insert(kept.triangle_face.end(),
            result.triangle_face.begin() + static_cast<std::ptrdiff_t>(begin),
            result.triangle_face.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return kept;
}

} // namespace MayaFlux::Kinesis
