#include "PlacementInputs.hpp"

namespace MayaFlux::Portal::Forma {

std::optional<Kinesis::AABB2D> region_of(const RectPlacementInputs& placement, std::string& error)
{
    const glm::vec4 rect = placement.rect;
    if (!(rect.z > 0.F) || !(rect.w > 0.F)) {
        error = "rect needs a positive width and height";
        return std::nullopt;
    }
    return Kinesis::AABB2D::from_origin_size({ rect.x, rect.y }, { rect.z, rect.w });
}

std::optional<Kinesis::AABB2D> region_of(const RadialPlacementInputs& placement, std::string& error)
{
    if (!(placement.radius > 0.F)) {
        error = "radius needs to be positive";
        return std::nullopt;
    }
    return Kinesis::AABB2D::from_ndc(placement.center, glm::vec2(placement.radius));
}

}
