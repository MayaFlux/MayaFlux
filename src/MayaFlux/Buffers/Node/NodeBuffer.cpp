#include "NodeBuffer.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {
NodeSourceProcessor::NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix, bool clear_before_process)
    : m_node(node)
    , m_mix(mix)
    , m_clear_before_process(clear_before_process)
{
}

void NodeSourceProcessor::process(std::shared_ptr<AudioBuffer> buffer)
{
    if (!m_node) {
        std::cerr << "Warning: NodeSourceProcessor has null node" << std::endl;
        return;
    }

    try {
        std::vector<double> node_data = m_node->process_batch(buffer->get_num_samples());
        auto& buffer_data = buffer->get_data();

        bool should_clear = m_clear_before_process;
        if (auto node_buffer = std::dynamic_pointer_cast<NodeBuffer>(buffer)) {
            should_clear = node_buffer->get_clear_before_process();
        }

        if (should_clear) {
            buffer->clear();
        }

        for (size_t i = 0; i < std::min(node_data.size(), buffer_data.size()); i++) {
            buffer_data[i] += node_data[i] * m_mix;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error processing node: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error processing node" << std::endl;
    }
}

NodeBuffer::NodeBuffer(u_int32_t channel_id, u_int32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process)
    : StandardAudioBuffer(channel_id, num_samples)
    , m_source_node(source)
    , m_clear_before_process(clear_before_process)
{
    m_default_processor = create_default_processor();
}

void NodeBuffer::process_default()
{
    if (m_clear_before_process) {
        clear();
    }
    m_default_processor->process(shared_from_this());
}

std::shared_ptr<BufferProcessor> NodeBuffer::create_default_processor()
{
    return std::make_shared<NodeSourceProcessor>(m_source_node);
}
}
