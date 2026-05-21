#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Portal/Text/InkPress.hpp"

namespace MayaFlux::Portal::Forma {

std::optional<InspectResult> Inspector::s_node_graph_result;
std::optional<InspectResult> Inspector::s_buffer_result;
std::optional<InspectResult> Inspector::s_scheduler_result;
std::optional<InspectResult> Inspector::s_event_result;

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

void Inspector::destroy(InspectResult& result)
{
    if (s_node_graph_result && &result == &*s_node_graph_result) {
        s_node_graph_result.reset();
        return;
    }
    if (s_buffer_result && &result == &*s_buffer_result) {
        s_buffer_result.reset();
        return;
    }
    if (s_scheduler_result && &result == &*s_scheduler_result) {
        s_scheduler_result.reset();
        return;
    }
    if (s_event_result && &result == &*s_event_result) {
        s_event_result.reset();
        return;
    }
    result = InspectResult {};
}

} // namespace MayaFlux::Portal::Forma
