#include "Tend.hpp"

namespace MayaFlux::Portal::Forma {

TextField create_text_field(
    Surface& surface,
    Kinesis::AABB2D bounds,
    std::shared_ptr<Portal::Text::PressParams> params,
    std::string initial_text,
    bool scrollable)
{
    if (!scrollable) {
        return TextField {}.place(
            internal::atelier().batch_slice(surface, Graphics::PrimitiveTopology::TRIANGLE_LIST),
            surface, bounds, std::move(params), std::move(initial_text));
    }

    auto viewport = Scrollable {}.place(surface, bounds);
    return TextField {}.scrollable(surface, viewport, std::move(params), std::move(initial_text));
}

} // namespace MayaFlux::Portal::Forma
