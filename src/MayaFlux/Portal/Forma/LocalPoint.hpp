#pragma once

#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Kinesis/Viewport/ViewTransform.hpp"

namespace MayaFlux::Kakshya {
struct MeshData;
}

namespace MayaFlux::Portal::Forma {

/**
 * @brief An Element::to_local for the z = 0 plane of a placed model.
 *
 * The result's x and y are plane coordinates, matching a Surface drawn with
 * view * model, and z is the world distance along the pointer ray. Only the
 * side local +z points to can be hit.
 *
 * @param view  View the plane is drawn with, read on each event.
 * @param model Placement of the plane in the world, read on each event.
 */
[[nodiscard]] MAYAFLUX_API std::function<std::optional<glm::vec3>(glm::vec2)> plane_to_local(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> model);

/**
 * @brief An Element::to_local for a mesh that shows an image across its UVs.
 *
 * The hit UV is mapped into @p bounds, v running down, so for a host element
 * with those bounds Surface::route_input_from() lands on the texel the mesh
 * shows there. z is the world distance along the pointer ray. Either face can
 * be hit.
 *
 * @param view   View the mesh is drawn with, read on each event.
 * @param model  Placement of the mesh in the world, read on each event.
 * @param mesh   Mesh with the canonical 68 byte vertex layout, read on each
 *               event. Another layout is reported once and never hit.
 * @param bounds The host element's rectangle.
 */
[[nodiscard]] MAYAFLUX_API std::function<std::optional<glm::vec3>(glm::vec2)> mesh_to_local(
    std::function<Kinesis::ViewTransform()> view,
    std::function<glm::mat4()> model,
    const std::shared_ptr<const Kakshya::MeshData>& mesh,
    Kinesis::AABB2D bounds);

} // namespace MayaFlux::Portal::Forma
