#include "ComputeOutNode.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes::GpuSync {

ComputeOutNode::ComputeOutNode(const std::shared_ptr<Buffers::VKBuffer>& buffer, size_t element_count)
    : m_gpu_buffer(buffer)
    , m_readback_data(element_count)
    , m_element_count(element_count)
{
    if (!m_gpu_buffer) {
        error<std::invalid_argument>(
            Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            std::source_location::current(),
            "Cannot create ComputeOutNode with null GPU buffer");
    }

    if (element_count == 0) {
        error<std::invalid_argument>(
            Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            std::source_location::current(),
            "Cannot create ComputeOutNode with zero element count");
    }

    size_t required_size = element_count * sizeof(double);
    if (m_gpu_buffer->get_size_bytes() < required_size) {
        MF_WARN(Journal::Component::Nodes, Journal::Context::NodeProcessing,
            "GPU buffer size ({} bytes) is smaller than requested element count ({} elements = {} bytes)",
            m_gpu_buffer->get_size_bytes(), element_count, required_size);
    }
}

void ComputeOutNode::compute_frame()
{
    Buffers::download_from_gpu(
        m_gpu_buffer,
        m_readback_data.data(),
        m_readback_data.size() * sizeof(double));

    m_last_output = m_readback_data[0];
}

[[nodiscard]] double ComputeOutNode::get_element(size_t index) const
{
    if (index >= m_readback_data.size()) {
        error<std::out_of_range>(
            Journal::Component::Nodes,
            Journal::Context::NodeProcessing,
            std::source_location::current(),
            "Element index {} out of range (size: {})",
            index, m_readback_data.size());
    }
    return m_readback_data[index];
}

void ComputeOutNode::update_context(double value)
{
    m_readback_float_buffer.resize(m_readback_data.size());

    std::ranges::transform(m_readback_data,
        m_readback_float_buffer.begin(),
        [](double d) { return static_cast<float>(d); });

    m_context.value = value;
    m_context.element_count = m_element_count;
    m_context.m_gpu_data = m_readback_float_buffer;
}
} // namespace MayaFlux::Nodes
