#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

RowBuffer Inspector::make_row_buffer(
    const std::shared_ptr<Core::Window>& window,
    std::string_view text,
    glm::uvec2 pixel_dims) const
{
    Portal::Text::PressParams params {
        .color = { 1.F, 1.F, 1.F, 1.F },
        .budget_h = pixel_dims.y,
    };
    auto text_image = Portal::Text::press(text, pixel_dims, params);

    const size_t cap = static_cast<const size_t>(12) * Kakshya::VertexLayout::for_meshes().stride_bytes;
    auto buf = std::make_shared<Buffers::FormaBuffer>(
        cap, Graphics::PrimitiveTopology::TRIANGLE_LIST);
    m_bm.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    buf->setup_rendering({
        .target_window = window,
        .additional_textures = { { "text", text_image } },
    });

    return { .buf = std::move(buf), .text_image = std::move(text_image) };
}

} // namespace MayaFlux::Portal::Forma
