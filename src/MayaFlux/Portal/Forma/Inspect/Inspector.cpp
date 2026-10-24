#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"

namespace MayaFlux::Portal::Forma {

namespace {
    constexpr size_t k_row_buffer_capacity = 4096;
}

std::shared_ptr<Buffers::FormaBuffer> Inspector::make_header_buffer(
    const std::shared_ptr<Core::Window>& window) const
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(
        k_row_buffer_capacity, Graphics::PrimitiveTopology::TRIANGLE_LIST);
    m_bm.add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    buf->setup_rendering({
        .target_window = window,
        .additional_textures = { { "text", nullptr } },
    });
    return buf;
}

std::vector<EntryBuffer> Inspector::make_row_buffers(
    const std::shared_ptr<Core::Window>& window,
    size_t count) const
{
    std::vector<EntryBuffer> rows;
    if (count == 0) {
        return rows;
    }

    const auto buf = make_header_buffer(window);

    rows.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        rows.push_back({ .buf = FormaTarget::slice_of(buf, Graphics::PrimitiveTopology::TRIANGLE_LIST) });
    }
    return rows;
}

void Inspector::destroy(InspectResult& result)
{
    if (m_node_graph_result && &result == &*m_node_graph_result) {
        m_node_graph_result.reset();
        return;
    }
    if (m_buffer_result && &result == &*m_buffer_result) {
        m_buffer_result.reset();
        return;
    }
    if (m_scheduler_result && &result == &*m_scheduler_result) {
        m_scheduler_result.reset();
        return;
    }
    if (m_event_result && &result == &*m_event_result) {
        m_event_result.reset();
        return;
    }
    result = InspectResult {};
}

} // namespace MayaFlux::Portal::Forma
