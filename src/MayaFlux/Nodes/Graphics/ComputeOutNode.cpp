#include "ComputeOutNode.hpp"
#include "MayaFlux/Buffers/Staging/StagingUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Nodes {

MayaFlux::Nodes::ComputeOutNode::ComputeOutNode(const std::shared_ptr<Buffers::VKBuffer>& buffer, size_t element_count)
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

double MayaFlux::Nodes::ComputeOutNode::process_sample(double /*input*/)
{
    Buffers::download_from_gpu(
        m_gpu_buffer,
        m_readback_data.data(),
        m_readback_data.size() * sizeof(double));

    m_last_output = m_readback_data[0];
    return m_last_output;
}

[[nodiscard]] double MayaFlux::Nodes::ComputeOutNode::get_element(size_t index) const
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

} // namespace MayaFlux::Nodes
