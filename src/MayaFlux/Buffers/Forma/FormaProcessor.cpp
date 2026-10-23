#include "FormaProcessor.hpp"

#include "MayaFlux/Buffers/Shaders/RenderProcessor.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Kakshya/NDData/VertexLayout.hpp"

namespace MayaFlux::Buffers {

namespace {

    uint32_t stride_for(Portal::Graphics::PrimitiveTopology topology)
    {
        switch (topology) {
        case Portal::Graphics::PrimitiveTopology::POINT_LIST:
            return Kakshya::VertexLayout::for_points().stride_bytes;
        case Portal::Graphics::PrimitiveTopology::LINE_LIST:
        case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
            return Kakshya::VertexLayout::for_lines().stride_bytes;
        default:
            return Kakshya::VertexLayout::for_meshes().stride_bytes;
        }
    }

}

FormaProcessor::FormaProcessor(Portal::Graphics::PrimitiveTopology topology)
    : m_topology(topology)
    , m_stride(stride_for(topology))
    , m_slices(m_stride, topology)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;
}

std::span<const uint8_t> FormaProcessor::whole_vertices(const std::vector<uint8_t>& bytes) const noexcept
{
    return { bytes.data(), bytes.size() - bytes.size() % m_stride };
}

void FormaProcessor::set_bytes(const std::vector<uint8_t>& bytes)
{
    if (m_default_slice == 0) {
        m_default_slice = m_slices.add(whole_vertices(bytes));
    } else {
        m_slices.update(m_default_slice, whole_vertices(bytes));
    }
    m_changed = true;
}

FormaProcessor::SliceId FormaProcessor::add_slice(
    const std::vector<uint8_t>& bytes,
    std::optional<Portal::Graphics::PrimitiveTopology> topology)
{
    m_changed = true;
    return m_slices.add(whole_vertices(bytes), topology);
}

void FormaProcessor::update_slice(SliceId id, const std::vector<uint8_t>& bytes)
{
    m_slices.update(id, whole_vertices(bytes));
    m_changed = true;
}

void FormaProcessor::remove_slice(SliceId id)
{
    if (id == m_default_slice) {
        m_default_slice = 0;
    }
    m_slices.remove(id);
    m_changed = true;
}

void FormaProcessor::set_slice_visible(SliceId id, bool visible)
{
    m_slices.set_visible(id, visible);
    m_changed = true;
}

void FormaProcessor::bring_slice_to_front(SliceId id)
{
    m_slices.bring_to_front(id);
    m_changed = true;
}

void FormaProcessor::send_slice_to_back(SliceId id)
{
    m_slices.send_to_back(id);
    m_changed = true;
}

bool FormaProcessor::has_pending() const noexcept
{
    return m_changed;
}

void FormaProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk) {
        error<std::invalid_argument>(
            Journal::Component::Buffers,
            Journal::Context::BufferProcessing,
            std::source_location::current(),
            "FormaProcessor requires a VKBuffer");
    }

    m_staging = create_staging_buffer(vk->get_size_bytes());

    switch (m_topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        vk->set_vertex_layout(Kakshya::VertexLayout::for_points());
        break;
    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        vk->set_vertex_layout(Kakshya::VertexLayout::for_lines());
        break;
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
    default:
        vk->set_vertex_layout(Kakshya::VertexLayout::for_meshes());
        break;
    }
}

void FormaProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_staging.reset();
    m_slices = SliceSet(m_stride, m_topology);
    m_default_slice = 0;
    m_changed = false;
    m_pending_runs.reset();
}

void FormaProcessor::set_texture(std::shared_ptr<Core::VKImage> image, std::string binding)
{
    m_pending_texture = PendingTexture { .image = std::move(image), .binding = std::move(binding) };
    m_texture_dirty.test_and_set(std::memory_order_release);
}

void FormaProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (m_texture_dirty.test(std::memory_order_acquire)) {
        auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
        if (vk) {
            if (auto rp = vk->get_render_processor()) {
                if (m_pending_texture) {
                    rp->bind_texture(m_pending_texture->binding,
                        m_pending_texture->image);
                }
                m_pending_texture.reset();
                m_texture_dirty.clear(std::memory_order_release);
            }
        }
    }

    if (!m_changed && !m_pending_runs)
        return;

    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk)
        return;

    if (m_changed) {
        m_changed = false;
        m_slices.flush(vk, m_staging);
        if (auto runs = m_slices.take_runs()) {
            m_pending_runs = std::move(runs);
        }
    }

    if (m_pending_runs) {
        apply_runs(vk);
    }
}

void FormaProcessor::apply_runs(const std::shared_ptr<VKBuffer>& buffer)
{
    const auto total = m_slices.vertex_count();
    const auto run_count = m_pending_runs->size();
    const Portal::Graphics::DrawRun first = run_count == 1 ? m_pending_runs->front() : Portal::Graphics::DrawRun {};
    const bool whole = run_count == 1 && first.vertex_offset == 0 && first.vertex_count == total;

    if (auto layout = buffer->get_vertex_layout()) {
        layout->vertex_count = run_count == 0 ? 0U : total;
        buffer->set_vertex_layout(*layout);
    }

    auto render = buffer->get_render_processor();
    if (!render) {
        if (whole || run_count == 0) {
            m_pending_runs.reset();
        }
        return;
    }

    if (run_count > 1) {
        render->set_runs(std::move(*m_pending_runs));
        render->set_vertex_range(0, 0);
    } else {
        if (run_count == 1 && first.topology != m_topology) {
            render->set_runs({ first });
        } else {
            render->set_runs({});
        }
        if (run_count == 1 && !whole) {
            render->set_vertex_range(first.vertex_offset, first.vertex_count);
        } else {
            render->set_vertex_range(0, 0);
        }
    }

    m_pending_runs.reset();
}

} // namespace MayaFlux::Buffers
