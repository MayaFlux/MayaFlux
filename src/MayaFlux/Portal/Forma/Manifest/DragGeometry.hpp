#pragma once

#include "DragGeometryInputs.hpp"
#include "FactoryTable.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief A float state Form: an outline scaled along one axis by the state.
 *
 * The outline is filled. The hit region is the placement's rect, so a track
 * stays clickable at zero, and a drag along the scaled axis writes the
 * fraction of the rect under the pointer.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Geometry::Form<float>> scale_form(
    const ScaleInputs& input, const RectPlacementInputs& placement, std::string& error);

/**
 * @brief A Form of unit square points that a press grabs and a drag moves.
 *
 * The state is the list of points, initially the input's. The hit region is
 * the placement's rect. See Geometry::handle_path.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Geometry::Form<std::vector<glm::vec2>>> handle_path_form(
    const HandlePathInputs& input, const RectPlacementInputs& placement, std::string& error);

/**
 * @brief Register scale_form under a name.
 *
 * The factory element needs points. The instance needs a rect. The initial
 * state is the element's value member.
 */
template <typename Source, typename Access>
void expose_scale(FactoryTable<Source, Access>& table, std::string name)
{
    Reflect::RecordBinding<ScaleInputs, Source, Access> input;
    input.required("points");

    table.template expose_typed<float>(std::move(name),
        std::move(input), rect_placement_binding<Source, Access>(), scale_form,
        &ScaleInputs::value);
}

/**
 * @brief Register handle_path_form under a name.
 *
 * The factory element needs points. The instance needs a rect. The initial
 * state is the element's points.
 */
template <typename Source, typename Access>
void expose_handle_path(FactoryTable<Source, Access>& table, std::string name)
{
    Reflect::RecordBinding<HandlePathInputs, Source, Access> input;
    input.required("points");

    table.template expose_typed<std::vector<glm::vec2>>(std::move(name),
        std::move(input), rect_placement_binding<Source, Access>(), handle_path_form,
        &HandlePathInputs::points);
}

}
