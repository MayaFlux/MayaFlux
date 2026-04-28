#include "FormaProcessor.hpp"

#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"
#include "MayaFlux/Journal/Archivist.hpp"
#include "MayaFlux/Nodes/Graphics/VertexSpec.hpp"

namespace MayaFlux::Buffers {

FormaProcessor::FormaProcessor(Portal::Graphics::PrimitiveTopology topology)
    : m_topology(topology)
{
    m_processing_token = ProcessingToken::GRAPHICS_BACKEND;

    switch (topology) {
    case Portal::Graphics::PrimitiveTopology::POINT_LIST:
        m_stride = sizeof(Nodes::PointVertex);
        break;
    case Portal::Graphics::PrimitiveTopology::LINE_LIST:
    case Portal::Graphics::PrimitiveTopology::LINE_STRIP:
        m_stride = sizeof(Nodes::LineVertex);
        break;
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_LIST:
    case Portal::Graphics::PrimitiveTopology::TRIANGLE_STRIP:
    default:
        m_stride = sizeof(Nodes::MeshVertex);
        break;
    }
}

void FormaProcessor::set_bytes(std::vector<uint8_t> bytes)
{
    m_pending = std::move(bytes);
    m_dirty.test_and_set(std::memory_order_release);
}

bool FormaProcessor::has_pending() const noexcept
{
    return m_dirty.test(std::memory_order_acquire);
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
    m_pending.clear();
    m_active.clear();
}

void FormaProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_dirty.test(std::memory_order_acquire))
        return;

    m_dirty.clear(std::memory_order_release);
    std::swap(m_active, m_pending);

    if (m_active.empty())
        return;

    auto vk = std::dynamic_pointer_cast<VKBuffer>(buffer);
    if (!vk)
        return;

    const size_t required = m_active.size();
    const size_t available = vk->get_size_bytes();

    if (required > available) {
        vk->resize(static_cast<size_t>(required * 1.5F), false);
        m_staging = create_staging_buffer(vk->get_size_bytes());
    }

    upload_to_gpu(
        m_active.data(),
        std::min(required, vk->get_size_bytes()),
        vk,
        m_staging);

    auto layout = vk->get_vertex_layout();
    if (layout) {
        auto updated = *layout;
        updated.vertex_count = vertex_count(m_active.size());
        vk->set_vertex_layout(updated);
    }
}

uint32_t FormaProcessor::vertex_count(size_t byte_count) const noexcept
{
    if (m_stride == 0)
        return 0;
    return static_cast<uint32_t>(byte_count / m_stride);
}

} // namespace MayaFlux::Buffers
