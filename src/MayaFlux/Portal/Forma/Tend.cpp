#include "Tend.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    std::shared_ptr<Buffers::FormaBuffer> make_text_buffer(Surface& surface)
    {
        return internal::atelier().create_buffer(
            surface.window(), internal::k_capacity_bytes,
            Graphics::PrimitiveTopology::TRIANGLE_LIST, {},
            std::vector<std::pair<std::string, std::shared_ptr<Core::VKImage>>> {
                { "text", nullptr } });
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

    auto viewport_buf = internal::atelier().create_buffer(
        surface.window(), internal::k_capacity_bytes, Graphics::PrimitiveTopology::TRIANGLE_STRIP);
    auto viewport = Scrollable {}.place(std::move(viewport_buf), surface, bounds);

    return TextField {}.scrollable(
        make_text_buffer(surface), surface, viewport, std::move(params), std::move(initial_text));
}

} // namespace MayaFlux::Portal::Forma
