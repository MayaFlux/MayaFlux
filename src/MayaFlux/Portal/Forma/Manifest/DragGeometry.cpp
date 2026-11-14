#include "DragGeometry.hpp"

#include "Containment.hpp"
#include "MayaFlux/Kinesis/Geometry2D.hpp"
#include "MayaFlux/Portal/Forma/Primitives/FormFactory.hpp"

namespace MayaFlux::Portal::Forma {

std::optional<Geometry::Form<float>> scale_form(
    const ScaleInputs& input, const RectPlacementInputs& placement, std::string& error)
{
    if (input.points.size() < 3) {
        error = "points needs at least three x y pairs";
        return std::nullopt;
    }
    const auto region = region_of(placement, error);
    if (!region)
        return std::nullopt;

    const auto triangles = Kinesis::triangulate_polygon(input.points, {});
    if (triangles.empty()) {
        error = "the outline does not form a valid region";
        return std::nullopt;
    }

    const size_t capacity = triangles.size() * 3 * sizeof(Kakshya::MeshVertex);
    auto fn = geometry_with_containment<float>(
        [unit = input.points, triangles, region = *region, color = input.color,
            horizontal = input.horizontal](const float& value, std::vector<uint8_t>& out) {
            const float scale = std::clamp(value, 0.F, 1.F);
            std::vector<Kakshya::Vertex> verts;
            verts.reserve(triangles.size() * 3);
            for (const auto& triangle : triangles) {
                for (const uint32_t k : triangle) {
                    glm::vec2 point = unit[k];
                    (horizontal ? point.x : point.y) *= scale;
                    const glm::vec2 ndc = region.point_at(point);
                    verts.push_back(Kakshya::Vertex {
                        .position = { ndc.x, ndc.y, 0.F },
                        .color = color,
                    });
                }
            }
            Geometry::write_verts(out, Kakshya::to_mesh_vertices(verts));
        },
        rect_containment(*region));

    Geometry::Form<float> form { std::move(fn), Graphics::PrimitiveTopology::TRIANGLE_LIST, capacity };
    form.wire = Geometry::drag_with<float>(
        Kinesis::axis_fraction(*region, 0.F, input.horizontal));
    return form;
}

std::optional<Geometry::Form<std::vector<glm::vec2>>> handle_path_form(
    const HandlePathInputs& input, const RectPlacementInputs& placement, std::string& error)
{
    if (input.points.size() < 2) {
        error = "points needs at least two x y pairs";
        return std::nullopt;
    }
    for (const glm::vec2 point : input.points) {
        if (point.x < 0.F || point.x > 1.F || point.y < 0.F || point.y > 1.F) {
            error = "points must lie in the unit square";
            return std::nullopt;
        }
    }
    if (!(input.thickness >= 0.F) || !(input.handle_size >= 0.F)) {
        error = "thickness and handle_size may not be negative";
        return std::nullopt;
    }
    if (!(input.grab_radius > 0.F)) {
        error = "grab_radius needs to be positive";
        return std::nullopt;
    }
    const auto region = region_of(placement, error);
    if (!region)
        return std::nullopt;

    return Geometry::handle_path(*region, input.points.size(), input.path_color,
        input.handle_color, input.thickness, input.handle_size, input.grab_radius);
}

}
