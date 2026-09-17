#include "Element.hpp"

#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Kinesis/GeometryPrimitives.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief weight=1 is this pipeline's own convention, not Kinesis's: it
    ///        is what forma_multi.frag reads to select textures[0]. Kinesis::
    ///        textured_mesh_rect() has no opinion on what the value means -
    ///        that domain knowledge stays here, at the one place it applies.
    constexpr float k_single_texture_weight = 1.F;

} // namespace

Element& Element::with_texture(
    const std::shared_ptr<Core::VKImage>& image,
    Kinesis::AABB2D region)
{
    if (!buffer)
        return *this;

    texture = image;
    buffer->submit(Kinesis::textured_mesh_rect(region, k_single_texture_weight));
    buffer->bind_texture(0, image);
    return *this;
}

Element& Element::with_texture(
    const std::shared_ptr<Buffers::TextureBuffer>& buf,
    Kinesis::AABB2D region)
{
    return with_texture(buf->get_texture(), region);
}

Element& Element::with_text(
    std::string_view text,
    std::optional<Portal::Text::PressParams> params,
    Kinesis::AABB2D region)
{
    if (!buffer)
        return *this;

    if (!params.has_value())
        params = Portal::Text::PressParams {};

    texture = Portal::Text::press(text, params->render_bounds, *params);
    buffer->submit(Kinesis::textured_mesh_rect(region, k_single_texture_weight));
    buffer->bind_texture(0, texture);
    return *this;
}

void Element::set_text(std::string_view text, std::optional<Portal::Text::PressParams> params)
{
    if (!texture)
        return;

    if (!params.has_value())
        params = Portal::Text::PressParams {};

    Portal::Text::repress(texture, text, *params);
    if (buffer)
        buffer->bind_texture(0, texture);
}

Element& Element::retarget(Kinesis::AABB2D region)
{
    if (!buffer)
        return *this;

    buffer->submit(Kinesis::textured_mesh_rect(region, k_single_texture_weight));
    return *this;
}

} // namespace MayaFlux::Portal::Forma
