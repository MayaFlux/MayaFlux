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
            internal::atelier().create_batch(surface), surface, bounds, std::move(params), std::move(initial_text));
    }

    const auto batch = internal::atelier().create_batch(surface);
    auto viewport_target = FormaTarget::slice_of(batch, Graphics::PrimitiveTopology::TRIANGLE_STRIP);
    auto viewport = Scrollable {}.place(std::move(viewport_target), surface, bounds);

    return TextField {}.scrollable(
        FormaTarget::slice_of(batch, Graphics::PrimitiveTopology::TRIANGLE_LIST),
        surface, viewport, std::move(params), std::move(initial_text));
}

} // namespace MayaFlux::Portal::Forma
