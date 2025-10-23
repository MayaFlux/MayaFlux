#include "NodeBuffer.hpp"
#include "MayaFlux/Nodes/Node.hpp"

namespace MayaFlux::Buffers {
NodeSourceProcessor::NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix, bool clear_before_process)
    : m_node(node)
    , m_mix(mix)
    , m_clear_before_process(clear_before_process)
{
}

void NodeSourceProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_node) {
        std::cerr << "Warning: NodeSourceProcessor has null node" << std::endl;
        return;
    }

    try {
        auto& buffer_data = std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_data();

        bool should_clear = m_clear_before_process;
        if (auto node_buffer = std::dynamic_pointer_cast<NodeBuffer>(buffer)) {
            should_clear = node_buffer->get_clear_before_process();
        }

        if (should_clear) {
            buffer->clear();
        }

        m_node->save_state();
        for (size_t i = 0; i < buffer_data.size(); i++) {
            buffer_data[i] += m_node->process_sample(0.f) * m_mix;
        }
        m_node->restore_state();

    } catch (const std::exception& e) {
        std::cerr << "Error processing node: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error processing node" << std::endl;
    }
}

std::vector<double> NodeSourceProcessor::get_node_data(uint32_t num_samples)
{
    std::vector<double> output(num_samples);

    m_node->save_state();
    for (size_t i = 0; i < num_samples; i++) {
        output[i] = m_node->process_sample(0.f);
    }
    m_node->restore_state();
    return output;
}

NodeBuffer::NodeBuffer(uint32_t channel_id, uint32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process)
    : AudioBuffer(channel_id, num_samples)
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
