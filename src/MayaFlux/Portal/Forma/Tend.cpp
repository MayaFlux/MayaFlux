#include "Tend.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    std::shared_ptr<Buffers::FormaBuffer> make_text_buffer(Surface& surface)
    {
        return internal::atelier().create_buffer(
            surface.render_config({ .additional_textures = { { "text", nullptr } } }));
    }

} // namespace

TextField create_text_field(
    Surface& surface,
    Kinesis::AABB2D bounds,
    std::shared_ptr<Portal::Text::PressParams> params,
    std::string initial_text,
    bool scrollable)
{
    if (!scrollable) {
        return TextField {}.place(
            make_text_buffer(surface), surface, bounds, std::move(params), std::move(initial_text));
    }

    auto viewport_target = FormaTarget::slice_of(
        internal::atelier().create_buffer(surface.render_config()),
        Graphics::PrimitiveTopology::TRIANGLE_STRIP);
    auto viewport = Scrollable {}.place(std::move(viewport_target), surface, bounds);

    return TextField {}.scrollable(
        make_text_buffer(surface), surface, viewport, std::move(params), std::move(initial_text));
}

} // namespace MayaFlux::Portal::Forma
