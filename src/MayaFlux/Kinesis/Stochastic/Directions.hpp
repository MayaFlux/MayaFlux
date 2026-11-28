#pragma once

#include <glm/glm.hpp>

/**
 * Maps from the unit square onto shapes and sets of directions, each with its
 * density. Directions are in a frame whose +Z is the pole.
 */
namespace MayaFlux::Kinesis::Stochastic {

// =============================================================================
// Frame
// =============================================================================

/**
 * @brief A rotation whose third column is @p normal (Duff and others).
 * @param normal Unit vector.
 * @return Tangent, bitangent and normal as columns.
 */
[[nodiscard]] inline glm::mat3 orthonormal_basis(const glm::vec3& normal) noexcept
{
    const float sign = std::copysign(1.0F, normal.z);
    const float a = -1.0F / (sign + normal.z);
    const float b = normal.x * normal.y * a;
    return {
        glm::vec3(1.0F + sign * normal.x * normal.x * a, sign * b, -sign * normal.x),
        glm::vec3(b, sign + normal.y * normal.y * a, -normal.y),
        normal
    };
}

// =============================================================================
// Maps
// =============================================================================

/** @brief Uniform over the unit disk, keeping neighbours together (Shirley and Chiu). */
[[nodiscard]] inline glm::vec2 square_to_disk(const glm::vec2& u) noexcept
{
    const glm::vec2 centred = 2.0F * u - 1.0F;
    if (centred.x == 0.0F && centred.y == 0.0F) {
        return { 0.0F, 0.0F };
    }
    constexpr float quarter = std::numbers::pi_v<float> / 4.0F;
    const bool wide = std::abs(centred.x) > std::abs(centred.y);
    const float radius = wide ? centred.x : centred.y;
    const float theta = wide
        ? quarter * (centred.y / centred.x)
        : 2.0F * quarter - quarter * (centred.x / centred.y);
    return { radius * std::cos(theta), radius * std::sin(theta) };
}

/**
 * @brief Uniform over the directions within an angle of +Z.
 * @param cos_max Cosine of that angle. -1 covers the sphere, 0 the hemisphere.
 */
[[nodiscard]] inline glm::vec3 square_to_cone(const glm::vec2& u, float cos_max) noexcept
{
    const float z = 1.0F - u.x * (1.0F - cos_max);
    const float radius = std::sqrt(std::max(1.0F - z * z, 0.0F));
    const float phi = 2.0F * std::numbers::pi_v<float> * u.y;
    return { radius * std::cos(phi), radius * std::sin(phi), z };
}

/** @brief Uniform over the sphere. */
[[nodiscard]] inline glm::vec3 square_to_sphere(const glm::vec2& u) noexcept
{
    return square_to_cone(u, -1.0F);
}

/** @brief Uniform over the +Z hemisphere. */
[[nodiscard]] inline glm::vec3 square_to_hemisphere(const glm::vec2& u) noexcept
{
    return square_to_cone(u, 0.0F);
}

/** @brief Over the +Z hemisphere in proportion to the cosine from +Z. */
[[nodiscard]] inline glm::vec3 square_to_cosine_hemisphere(const glm::vec2& u) noexcept
{
    const glm::vec2 disk = square_to_disk(u);
    return { disk.x, disk.y, std::sqrt(std::max(1.0F - disk.x * disk.x - disk.y * disk.y, 0.0F)) };
}

/** @brief Barycentric coordinates uniform over a triangle's area. */
[[nodiscard]] inline glm::vec3 square_to_barycentric(const glm::vec2& u) noexcept
{
    const float root = std::sqrt(u.x);
    const float first = 1.0F - root;
    const float second = u.y * root;
    return { first, second, 1.0F - first - second };
}

// =============================================================================
// Densities, per solid angle
// =============================================================================

/** @brief Density of square_to_cone(). */
[[nodiscard]] inline float cone_density(float cos_max) noexcept
{
    return 1.0F / (2.0F * std::numbers::pi_v<float> * std::max(1.0F - cos_max, 1.0e-7F));
}

/** @brief Density of square_to_cosine_hemisphere(). */
[[nodiscard]] inline float cosine_hemisphere_density(float cos_theta) noexcept
{
    return std::max(cos_theta, 0.0F) * std::numbers::inv_pi_v<float>;
}

/**
 * @brief Veach's power heuristic weight for one of two sampling methods.
 * @param chosen Density of the method that made the sample, times its sample count.
 * @param other  The same for the other method.
 */
[[nodiscard]] inline float power_heuristic(float chosen, float other) noexcept
{
    const float largest = std::max(chosen, other);
    if (!(largest > 0.0F)) {
        return 0.0F;
    }
    const float a = (chosen / largest) * (chosen / largest);
    const float b = (other / largest) * (other / largest);
    return a / (a + b);
}

} // namespace MayaFlux::Kinesis::Stochastic
