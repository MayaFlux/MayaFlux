#include "LocalPoint.hpp"

#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/MeshData.hpp"
#include "MayaFlux/Kinesis/Spatial/HitTest.hpp"

namespace MayaFlux::Portal::Forma {

std::function<std::optional<glm::vec3>(glm::vec2)> plane_to_local(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> model)
{
    return [view = std::move(view), model = std::move(model)](glm::vec2 screen) -> std::optional<glm::vec3> {
        const Kinesis::Ray ray = Kinesis::transform_ray(
            glm::inverse(model()), Kinesis::ray_through(screen, view()));

        if (!(ray.direction.z < 0.0F))
            return std::nullopt;

        const auto t = Kinesis::ray_plane_parameter(ray, glm::vec3(0.0F), { 0.0F, 0.0F, 1.0F });
        if (!t)
            return std::nullopt;

        const glm::vec3 met = ray.origin + ray.direction * *t;
        return glm::vec3(met.x, met.y, *t);
    };
}

std::function<std::optional<glm::vec3>(glm::vec2)> mesh_to_local(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> model,
    const std::shared_ptr<const Kakshya::MeshData>& mesh,
    Kinesis::AABB2D bounds)
{
    return [view = std::move(view), model = std::move(model), mesh, bounds,
               reported = std::make_shared<bool>(false)](glm::vec2 screen) -> std::optional<glm::vec3> {
        if (!mesh)
            return std::nullopt;

        const auto* bytes = std::get_if<std::vector<uint8_t>>(&mesh->vertex_variant);
        const auto* indices = std::get_if<std::vector<uint32_t>>(&mesh->index_variant);
        if (!bytes || !indices || mesh->layout.stride_bytes != sizeof(Kakshya::Vertex)) {
            if (!*reported) {
                *reported = true;
                MF_RT_WARN(Journal::Component::Portal, Journal::Context::API,
                    "mesh_to_local: the mesh does not have the canonical vertex layout");
            }
            return std::nullopt;
        }

        const std::span<const Kakshya::Vertex> vertices(
            reinterpret_cast<const Kakshya::Vertex*>(bytes->data()),
            bytes->size() / sizeof(Kakshya::Vertex));

        const Kinesis::Ray ray = Kinesis::transform_ray(
            glm::inverse(model()), Kinesis::ray_through(screen, view()));

        const auto hit = Kinesis::ray_triangles_parameter(
            ray, vertices, std::span<const uint32_t>(*indices), &Kakshya::Vertex::position);
        if (!hit)
            return std::nullopt;

        const auto& [triangle, at] = *hit;
        const size_t first = static_cast<size_t>(triangle) * 3;
        const glm::vec2 uv = vertices[(*indices)[first]].uv * (1.0F - at.y - at.z)
            + vertices[(*indices)[first + 1]].uv * at.y
            + vertices[(*indices)[first + 2]].uv * at.z;

        const glm::vec2 size = bounds.max - bounds.min;
        return glm::vec3(bounds.min.x + uv.x * size.x, bounds.max.y - uv.y * size.y, at.x);
    };
}

} // namespace MayaFlux::Portal::Forma
