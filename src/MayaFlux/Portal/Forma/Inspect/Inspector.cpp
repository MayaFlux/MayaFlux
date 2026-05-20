#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

#include "MayaFlux/Core/Backends/Windowing/Window.hpp"

#include "MayaFlux/Portal/Text/TypeFaceFoundry.hpp"

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

glm::uvec2 Inspector::row_pixel_dims(
    const std::shared_ptr<Core::Window>& window,
    float x_min, float x_max, float row_h) const
{
    const auto& ws = window->get_state();
    const auto w = static_cast<uint32_t>(
        (x_max - x_min) * 0.5F * static_cast<float>(ws.current_width));
    const auto h = static_cast<uint32_t>(
        row_h * 0.5F * static_cast<float>(ws.current_height));

    auto atlas = Text::TypeFaceFoundry::instance().get_default_glyph_atlas();

    if (!atlas) {
        error<std::runtime_error>(Journal::Component::Portal, Journal::Context::Runtime,
            std::source_location::current(),
            "Failed to get default glyph atlas for row pixel dimension calculation");
    }

    const uint32_t min_h = atlas->pixel_size();
    return { std::max(w, 1U), std::max(h, min_h) };
}

} // namespace MayaFlux::Portal::Forma
