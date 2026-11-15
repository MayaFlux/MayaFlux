#pragma once

#include "MayaFlux/IO/GlmTextCodec.hpp"
#include "MayaFlux/Kinesis/Spatial/Bounds.hpp"
#include "MayaFlux/Transitive/Reflect/RecordBinding.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @struct RectPlacementInputs
 * @brief A placement by origin and size: x, y, width, height.
 */
struct RectPlacementInputs {
    glm::vec4 rect {};
};

MF_DESCRIBE(RectPlacementInputs)

/**
 * @struct RadialPlacementInputs
 * @brief A placement by center and radius.
 */
struct RadialPlacementInputs {
    glm::vec2 center {};
    float radius {};
};

MF_DESCRIBE(RadialPlacementInputs)

}

#include "PlacementInputs.describe.hpp"

namespace MayaFlux::Portal::Forma {

/**
 * @brief The surface region a rect placement covers.
 * @param error Receives the reason when nullopt is returned: the width and
 * height must be positive.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Kinesis::AABB2D> region_of(
    const RectPlacementInputs& placement, std::string& error);

/**
 * @brief The surface region a radial placement covers: the box around its circle.
 * @param error Receives the reason when nullopt is returned: the radius must be positive.
 */
[[nodiscard]] MAYAFLUX_API std::optional<Kinesis::AABB2D> region_of(
    const RadialPlacementInputs& placement, std::string& error);

/** @brief A binding for RectPlacementInputs that requires rect. */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] Reflect::RecordBinding<RectPlacementInputs, Source, Access> rect_placement_binding()
{
    Reflect::RecordBinding<RectPlacementInputs, Source, Access> binding;
    binding.required("rect");
    return binding;
}

/** @brief A binding for RadialPlacementInputs that requires center and radius. */
template <typename Source, typename Access = Reflect::RecordAccess<Source>>
[[nodiscard]] Reflect::RecordBinding<RadialPlacementInputs, Source, Access> radial_placement_binding()
{
    Reflect::RecordBinding<RadialPlacementInputs, Source, Access> binding;
    binding.required("center").required("radius");
    return binding;
}

}
