#include "Accumulate.hpp"

#include "Attenuation.hpp"

#include "MayaFlux/Kinesis/ForEachIndexed.hpp"

namespace MayaFlux::Kinesis {

namespace {

    constexpr size_t k_receiver_grain = 256;

    /** A source as one receiver meets it. */
    struct Arrival {
        glm::vec3 radiance { 0.0F }; ///< Irradiance for placed points, radiance for shapes.
        glm::vec3 center { 0.0F };
        glm::vec3 toward { 0.0F };
        float distance {};
        float irradiance {}; ///< Per unit radiance, shapes only.
    };

    bool is_shape(RadiantShape shape) { return shape >= RadiantShape::Sphere; }

    bool is_planar(RadiantShape shape) { return shape == RadiantShape::Disk || shape == RadiantShape::Rectangle; }

    /** Radius of the sphere standing for a shape when a lobe is widened. */
    float stand_in_radius(const RadiantSource& source)
    {
        return source.shape == RadiantShape::Rectangle
            ? std::sqrt(source.extent.x * source.extent.y * std::numbers::inv_pi_v<float>)
            : source.extent.x;
    }

    std::optional<Arrival> arrival_of(const RadiantSource& source, const glm::vec3& position, const glm::vec3& normal)
    {
        Arrival arrival;
        if (source.shape == RadiantShape::Directional) {
            arrival.toward = -source.axis;
            arrival.radiance = source.color * source.intensity;
            return arrival;
        }

        arrival.center = source.position - position;
        arrival.distance = glm::length(arrival.center);
        if (!(arrival.distance > 1.0e-6F)) {
            return std::nullopt;
        }
        arrival.toward = arrival.center / arrival.distance;

        float reach = range_window(arrival.distance, source.range);
        if (!is_shape(source.shape)) {
            const float divisor = glm::dot(source.falloff,
                glm::vec3(1.0F, arrival.distance, arrival.distance * arrival.distance));
            if (divisor > 0.0F) {
                reach /= std::max(divisor, 1.0e-4F);
            }
            if (source.shape == RadiantShape::Spot) {
                reach *= angular_window(glm::dot(-arrival.toward, source.axis), source.cos_inner, source.cos_outer);
            }
        } else if (is_planar(source.shape) && !(glm::dot(arrival.center, source.axis) < 0.0F)) {
            return std::nullopt;
        }
        arrival.radiance = source.color * (source.intensity * reach);
        if (!is_shape(source.shape)) {
            return arrival;
        }

        const glm::vec3 up = glm::cross(source.right, source.axis);
        const float radius = source.extent.x;
        switch (source.shape) {
        case RadiantShape::Sphere:
            arrival.irradiance = sphere_irradiance(glm::dot(normal, arrival.toward),
                radius * radius / (arrival.distance * arrival.distance));
            break;
        case RadiantShape::Disk:
            arrival.irradiance = polygon_irradiance(disk_corners(arrival.center, source.right, up, radius), normal);
            break;
        case RadiantShape::Rectangle:
            arrival.irradiance = polygon_irradiance(
                rectangle_corners(arrival.center, source.right, up, source.extent), normal);
            break;
        default:
            arrival.irradiance = polygon_irradiance(
                tube_corners(arrival.center, source.right, radius, source.extent.y), normal);
            break;
        }
        return arrival;
    }

    glm::vec3 nearest_toward(const RadiantSource& source, const Arrival& arrival, const glm::vec3& ray)
    {
        const glm::vec3 up = glm::cross(source.right, source.axis);
        switch (source.shape) {
        case RadiantShape::Sphere:
            return sphere_toward(arrival.center, source.extent.x, ray);
        case RadiantShape::Disk:
            return disk_toward(arrival.center, source.axis, source.extent.x, ray);
        case RadiantShape::Rectangle:
            return rectangle_toward(arrival.center, source.right, up, source.extent, ray);
        default: {
            const glm::vec3 half = source.right * (0.5F * source.extent.y);
            return sphere_toward(
                segment_toward(arrival.center - half, arrival.center + half, ray), source.extent.x, ray);
        }
        }
    }

    bool hidden(const std::shared_ptr<const TriangleOccluder>& occluder, const RadiantSource& source,
        const glm::vec3& position, const glm::vec3& normal)
    {
        if (!occluder) {
            return false;
        }
        const glm::vec3 start = position + normal * occluder->clearance();
        const glm::vec3 end = source.shape == RadiantShape::Directional
            ? start - source.axis * (occluder->clearance() * 1.0e6F)
            : source.position;
        return occluder->occluded(start, end);
    }

}

glm::vec3 source_irradiance(
    const RadiantSource& source, const glm::vec3& position, const glm::vec3& normal) noexcept
{
    const auto arrival = arrival_of(source, position, normal);
    if (!arrival) {
        return glm::vec3(0.0F);
    }
    return arrival->radiance
        * (is_shape(source.shape) ? arrival->irradiance : std::max(glm::dot(normal, arrival->toward), 0.0F));
}

glm::vec3 source_radiance(
    const RadiantSource& source, const SurfaceResponse& surface,
    const glm::vec3& position, const glm::vec3& normal, const glm::vec3& view) noexcept
{
    const auto arrival = arrival_of(source, position, normal);
    if (!arrival) {
        return glm::vec3(0.0F);
    }

    if (!is_shape(source.shape)) {
        return surface_reflectance(surface, normal, view, arrival->toward)
            * arrival->radiance * std::max(glm::dot(normal, arrival->toward), 0.0F);
    }
    if (!(arrival->irradiance > 0.0F)) {
        return glm::vec3(0.0F);
    }

    const float radius = stand_in_radius(source);
    const float least = 0.5F * std::min(radius / arrival->distance, 1.0F);
    const glm::vec3 seen = glm::normalize(
        arrival->toward + normal * std::max(least - glm::dot(normal, arrival->toward), 0.0F));
    const glm::vec3 diffuse = surface_diffuse(surface, normal, view, seen) * arrival->irradiance;

    const glm::vec3 nearest = nearest_toward(source, *arrival, glm::reflect(-view, normal));
    const float reach = glm::length(nearest);
    const glm::vec3 direction = reach > 1.0e-6F ? nearest / reach : arrival->toward;

    const float alpha = roughness_to_alpha(surface.roughness);
    const float ratio = alpha / widened_alpha(alpha, radius, arrival->distance);
    const float energy = source.shape == RadiantShape::Tube ? ratio : ratio * ratio;
    const glm::vec3 specular = surface_specular(surface, normal, view, direction, alpha)
        * (energy * arrival->irradiance);
    const glm::vec3 mirror = schlick_fresnel(surface_f0(surface), std::max(glm::dot(normal, view), 0.0F));

    return (diffuse + glm::min(specular, mirror)) * arrival->radiance;
}

std::vector<glm::vec3> accumulate_irradiance(
    std::span<const RadiantSource> sources,
    std::span<const glm::vec3> positions, std::span<const glm::vec3> normals,
    const std::shared_ptr<const TriangleOccluder>& occluder)
{
    std::vector<glm::vec3> result(std::min(positions.size(), normals.size()), glm::vec3(0.0F));
    detail::for_each_indexed(
        std::execution::par, std::span(result),
        [&](size_t i, glm::vec3& total) {
            for (const auto& source : sources) {
                const glm::vec3 added = source_irradiance(source, positions[i], normals[i]);
                if (added != glm::vec3(0.0F) && !hidden(occluder, source, positions[i], normals[i])) {
                    total += added;
                }
            }
        },
        k_receiver_grain);
    return result;
}

std::vector<glm::vec3> accumulate_radiance(
    std::span<const RadiantSource> sources, const SurfaceResponse& surface,
    std::span<const glm::vec3> positions, std::span<const glm::vec3> normals, const glm::vec3& eye,
    const std::shared_ptr<const TriangleOccluder>& occluder)
{
    std::vector<glm::vec3> result(std::min(positions.size(), normals.size()), glm::vec3(0.0F));
    detail::for_each_indexed(
        std::execution::par, std::span(result),
        [&](size_t i, glm::vec3& total) {
            const glm::vec3 away = eye - positions[i];
            const float distance = glm::length(away);
            const glm::vec3 view = distance > 1.0e-6F ? away / distance : normals[i];
            for (const auto& source : sources) {
                const glm::vec3 added = source_radiance(source, surface, positions[i], normals[i], view);
                if (added != glm::vec3(0.0F) && !hidden(occluder, source, positions[i], normals[i])) {
                    total += added;
                }
            }
        },
        k_receiver_grain);
    return result;
}

} // namespace MayaFlux::Kinesis
