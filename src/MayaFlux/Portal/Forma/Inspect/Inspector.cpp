#include "Inspector.hpp"

#include "MayaFlux/Buffers/BufferManager.hpp"
#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Portal/Forma/Surface.hpp"

namespace MayaFlux::Portal::Forma {

namespace {
    constexpr size_t k_row_buffer_capacity = 4096;
}

Inspector::Inspector(const std::shared_ptr<Nodes::NodeGraphManager>& ngm,
    const std::shared_ptr<Buffers::BufferManager>& bm,
    const std::shared_ptr<Vruta::TaskScheduler>& sched,
    const std::shared_ptr<Vruta::EventManager>& event_mgr)
    : m_bm(bm)
    , m_ngm(ngm)
    , m_sched(sched)
    , m_event_mgr(event_mgr)
{
    if (!m_bm || !m_ngm || !m_sched || !m_event_mgr) {
        error<std::invalid_argument>(Journal::Component::Portal, Journal::Context::API,
            std::source_location::current(), "Forma::Inspector requires every manager");
    }
}

std::shared_ptr<Buffers::FormaBuffer> Inspector::make_header_buffer(
    const Surface& surface) const
{
    auto buf = std::make_shared<Buffers::FormaBuffer>(
        k_row_buffer_capacity, Graphics::PrimitiveTopology::TRIANGLE_LIST);
    m_bm->add_buffer(buf, Buffers::ProcessingToken::GRAPHICS_BACKEND);
    buf->setup_rendering(surface.render_config({
        .additional_textures = { { "text", nullptr } },
    }));

    if (surface.image()) {
        if (const auto processor = buf->get_render_processor()) {
            processor->set_target_area(surface.image(), surface.area());
        }
    }
    return buf;
}

std::vector<EntryBuffer> Inspector::make_row_buffers(
    const Surface& surface,
    size_t count) const
{
    std::vector<EntryBuffer> rows;
    if (count == 0) {
        return rows;
    }

    const auto buf = make_header_buffer(surface);

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
