#pragma once

#include "FactoryTable.hpp"
#include "StaticGeometryInputs.hpp"
#include "Containment.hpp"
#include "Interaction.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief A Form with no state that draws ordered vertices.
 *
 * Positions are x y in the unit square, mapped through the placement's rect
 * onto the surface, with z kept. The hit region is derived from the placed
 * vertices: inside the triangles for triangle topologies, the circumscribing
 * box otherwise.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Geometry::Form<std::monostate>> vertex_geometry_form(
    const VertexGeometryInputs& input, const RectPlacementInputs& placement, std::string& error);

/**
 * @struct PolygonFactory
 * @brief Factory for a bool state Form that fills or strokes an authored outline.
 *
 * Holes are cut out of a fill. The default hit region is the filled triangles,
 * or the box of a stroke. The names in containment and interaction are resolved in the tables
 * this value carries, which hold only what its builder registers.
 */
struct MAYAFLUX_API PolygonFactory {
    ContainmentTable containments;
    InteractionTable<bool> interactions;

    [[nodiscard]] std::optional<Geometry::Form<bool>> operator()(
        const PolygonInputs& input, const RectPlacementInputs& placement,
        std::string& error) const;
};

/**
 * @brief Register vertex_geometry_form under a name.
 *
 * The factory element needs a topology and one or more vertex children, with
 * an optional defaults child before them. The instance needs a rect.
 */
template <typename Source, typename Access>
void expose_vertex_geometry(FactoryTable<Source, Access>& table, std::string name)
{
    Reflect::RecordBinding<VertexGeometryInputs, Source, Access> input;
    input.required("topology")
        .template repeated<&VertexGeometryInputs::vertices>("vertex", "defaults", "position", 1);

    table.template expose_typed<std::monostate>(std::move(name),
        std::move(input), rect_placement_binding<Source, Access>(), vertex_geometry_form,
        [](const VertexGeometryInputs&, const RectPlacementInputs&, std::string&)
            -> std::optional<std::monostate> { return std::monostate {}; });
}

/**
 * @brief Register a PolygonFactory under a name.
 *
 * The factory element needs points and may hold hole children. The instance
 * needs a rect. The initial state is the element's on member.
 */
template <typename Source, typename Access>
void expose_polygon(FactoryTable<Source, Access>& table, std::string name,
    PolygonFactory factory)
{
    Reflect::RecordBinding<PolygonInputs, Source, Access> input;
    input.required("points")
        .template repeated<&PolygonInputs::holes>("hole", {}, "points");

    table.template expose_typed<bool>(std::move(name),
        std::move(input), rect_placement_binding<Source, Access>(), std::move(factory),
        &PolygonInputs::on);
}

}
