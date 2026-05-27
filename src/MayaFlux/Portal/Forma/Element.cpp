#include "Element.hpp"

#include "MayaFlux/Buffers/Textures/TextureBuffer.hpp"
#include "MayaFlux/Kakshya/NDData/VertexFormats.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

namespace {

    /// @brief Four MeshVertex quads in TRIANGLE_LIST order (two triangles) with
    ///        weight=1 so forma_multi.frag samples textures[0].
    std::array<Kakshya::MeshVertex, 6> textured_mesh_rect(
        MayaFlux::Kinesis::AABB2D region)
    {
        using V = MayaFlux::Kakshya::MeshVertex;
        const glm::vec2 mn = region.min;
        const glm::vec2 mx = region.max;
        return { {
            V { .position = { mn.x, mn.y, 0.F }, .weight = 1.F, .uv = { 0.F, 1.F } },
            V { .position = { mx.x, mn.y, 0.F }, .weight = 1.F, .uv = { 1.F, 1.F } },
            V { .position = { mn.x, mx.y, 0.F }, .weight = 1.F, .uv = { 0.F, 0.F } },
            V { .position = { mx.x, mn.y, 0.F }, .weight = 1.F, .uv = { 1.F, 1.F } },
            V { .position = { mx.x, mx.y, 0.F }, .weight = 1.F, .uv = { 1.F, 0.F } },
            V { .position = { mn.x, mx.y, 0.F }, .weight = 1.F, .uv = { 0.F, 0.F } },
        } };
    }

} // namespace

Element& Element::with_texture(
    const std::shared_ptr<Core::VKImage>& image,
    Kinesis::AABB2D region)
{
    if (!buffer)
        return *this;

    texture = image;
    buffer->submit(textured_mesh_rect(region));
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
    buffer->submit(textured_mesh_rect(region));
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

} // namespace MayaFlux::Portal::Forma
