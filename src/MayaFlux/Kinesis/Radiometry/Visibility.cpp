#include "Visibility.hpp"

#include "MayaFlux/Kinesis/ForEachIndexed.hpp"
#include "MayaFlux/Kinesis/Stochastic/Directions.hpp"
#include "MayaFlux/Kinesis/Stochastic/Sequences.hpp"

namespace MayaFlux::Kinesis {

TriangleOccluder::TriangleOccluder(std::span<const glm::vec3> positions, std::span<const uint32_t> indices)
    : m_positions(positions.begin(), positions.end())
    , m_indices(indices.begin(), indices.end())
    , m_hierarchy(triangle_boxes(m_positions, std::span<const uint32_t>(m_indices)))
{
    glm::vec3 low(std::numeric_limits<float>::max());
    glm::vec3 high(std::numeric_limits<float>::lowest());
    for (const auto& position : m_positions) {
        low = glm::min(low, position);
        high = glm::max(high, position);
    }
    m_clearance = m_positions.empty() ? 1.0e-4F : std::max(glm::length(high - low) * 1.0e-4F, 1.0e-6F);
}

bool TriangleOccluder::occluded(const glm::vec3& from, const glm::vec3& to) const
{
    const Ray ray { .origin = from, .direction = to - from };
    if (ray.direction == glm::vec3(0.0F)) {
        return false;
    }
    constexpr float limit = 1.0F - 1.0e-4F;
    return m_hierarchy.any(ray, limit, [this, &ray](uint32_t triangle) {
        const size_t at = static_cast<size_t>(triangle) * 3;
        const uint32_t a = m_indices[at];
        const uint32_t b = m_indices[at + 1];
        const uint32_t c = m_indices[at + 2];
        if (a >= m_positions.size() || b >= m_positions.size() || c >= m_positions.size()) {
            return false;
        }
        const auto hit = ray_triangle_parameter(ray, m_positions[a], m_positions[b], m_positions[c]);
        return hit && hit->x < limit;
    });
}

float ambient_visibility(
    const TriangleOccluder& occluder, const glm::vec3& point, const glm::vec3& normal,
    float range, uint32_t samples, uint32_t seed)
{
    if (samples == 0 || !(range > 0.0F)) {
        return 1.0F;
    }

    const glm::mat3 frame = Stochastic::orthonormal_basis(normal);
    const glm::vec3 start = point + normal * occluder.clearance();
    const glm::vec2 turn(Stochastic::hashed_unit(seed, 1U), Stochastic::hashed_unit(seed, 2U));

    uint32_t open = 0;
    for (uint32_t i = 0; i < samples; ++i) {
        const glm::vec2 u = glm::fract(Stochastic::hammersley_point(i, samples) + turn);
        const glm::vec3 direction = frame * Stochastic::square_to_cosine_hemisphere(u);
        open += occluder.occluded(start, start + direction * range) ? 0U : 1U;
    }
    return static_cast<float>(open) / static_cast<float>(samples);
}

std::vector<float> ambient_visibility(
    const TriangleOccluder& occluder, std::span<const glm::vec3> points, std::span<const glm::vec3> normals,
    float range, uint32_t samples)
{
    std::vector<float> result(std::min(points.size(), normals.size()), 1.0F);
    detail::for_each_indexed(
        std::execution::par, std::span(result),
        [&](size_t i, float& value) {
            value = ambient_visibility(occluder, points[i], normals[i], range, samples, static_cast<uint32_t>(i));
        },
        64);
    return result;
}

float source_visibility(
    const TriangleOccluder& occluder, const glm::vec3& point, const glm::vec3& normal,
    std::span<const glm::vec3> targets)
{
    if (targets.empty()) {
        return 1.0F;
    }

    const glm::vec3 start = point + normal * occluder.clearance();
    size_t seen = 0;
    for (const auto& target : targets) {
        seen += occluder.occluded(start, target) ? 0U : 1U;
    }
    return static_cast<float>(seen) / static_cast<float>(targets.size());
}

} // namespace MayaFlux::Kinesis
