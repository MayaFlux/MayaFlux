#pragma once

#include <glm/glm.hpp>

namespace MayaFlux::Kinesis {

/**
 * @brief Schlick's approximation of reflectance.
 * @param f0        Reflectance at normal incidence.
 * @param cos_theta Cosine of the incidence angle.
 */
[[nodiscard]] inline glm::vec3 schlick_fresnel(const glm::vec3& f0, float cos_theta) noexcept
{
    const float m = std::clamp(1.0F - cos_theta, 0.0F, 1.0F);
    const float m2 = m * m;
    return f0 + (glm::vec3(1.0F) - f0) * (m2 * m2 * m);
}

/** @brief Cosine weighted hemispherical average of schlick_fresnel(). */
[[nodiscard]] inline glm::vec3 schlick_average(const glm::vec3& f0) noexcept
{
    return (glm::vec3(1.0F) + 20.0F * f0) / 21.0F;
}

/**
 * @brief Unpolarised reflectance at a boundary between two dielectrics.
 * @param cos_theta Cosine of the incidence angle, in [0, 1].
 * @param eta       Index of the far side over index of the near side.
 * @return One under total internal reflection.
 */
[[nodiscard]] inline float dielectric_fresnel(float cos_theta, float eta) noexcept
{
    const float c = std::clamp(cos_theta, 0.0F, 1.0F);
    const float sin_t_sq = (1.0F - c * c) / (eta * eta);
    if (sin_t_sq >= 1.0F) {
        return 1.0F;
    }
    const float ct = std::sqrt(1.0F - sin_t_sq);
    const float parallel = (eta * c - ct) / (eta * c + ct);
    const float across = (c - eta * ct) / (c + eta * ct);
    return 0.5F * (parallel * parallel + across * across);
}

/**
 * @brief Unpolarised reflectance of a conductor, per channel.
 * @param cos_theta Cosine of the incidence angle, in [0, 1].
 * @param eta       Real part of the relative index.
 * @param k         Imaginary part of the relative index.
 */
[[nodiscard]] inline glm::vec3 conductor_fresnel(float cos_theta, const glm::vec3& eta, const glm::vec3& k) noexcept
{
    const float c = std::clamp(cos_theta, 0.0F, 1.0F);
    const float c2 = c * c;
    const float s2 = 1.0F - c2;

    const glm::vec3 t0 = eta * eta - k * k - s2;
    const glm::vec3 magnitude = glm::sqrt(t0 * t0 + 4.0F * eta * eta * k * k);
    const glm::vec3 t1 = magnitude + c2;
    const glm::vec3 a = glm::sqrt(glm::max(0.5F * (magnitude + t0), glm::vec3(0.0F)));
    const glm::vec3 t2 = 2.0F * c * a;
    const auto ratio = [](const glm::vec3& top, const glm::vec3& bottom) {
        return glm::vec3(
            bottom.x > 0.0F ? top.x / bottom.x : 1.0F,
            bottom.y > 0.0F ? top.y / bottom.y : 1.0F,
            bottom.z > 0.0F ? top.z / bottom.z : 1.0F);
    };
    const glm::vec3 across = ratio(t1 - t2, t1 + t2);

    const glm::vec3 t3 = c2 * magnitude + s2 * s2;
    const glm::vec3 t4 = t2 * s2;
    const glm::vec3 parallel = across * ratio(t3 - t4, t3 + t4);

    return 0.5F * (parallel + across);
}

} // namespace MayaFlux::Kinesis
