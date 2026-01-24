#include "NodeBuffer.hpp"

#include "MayaFlux/Nodes/Node.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

NodeSourceProcessor::NodeSourceProcessor(std::shared_ptr<Nodes::Node> node, float mix, bool clear_before_process)
    : m_node(std::move(node))
    , m_mix(mix)
    , m_clear_before_process(clear_before_process)
{
}

void NodeSourceProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_node) {
        m_node->add_buffer_reference();
    }
}

void NodeSourceProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_node) {
        m_node->remove_buffer_reference();
    }
}

void NodeSourceProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    if (!m_node) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing, "NodeSourceProcessor has null node. Skipping processing.");
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

        Buffers::update_buffer_with_node_data(m_node, buffer_data, m_mix);

    } catch (const std::exception& e) {
        error_rethrow(Journal::Component::Buffers, Journal::Context::BufferProcessing, std::source_location::current(), "Error processing node: {}", e.what());
    }
}

std::vector<double> NodeSourceProcessor::get_node_data(uint32_t num_samples)
{
    return Buffers::extract_multiple_samples(m_node, num_samples);
}

NodeBuffer::NodeBuffer(uint32_t channel_id, uint32_t num_samples, std::shared_ptr<Nodes::Node> source, bool clear_before_process)
    : AudioBuffer(channel_id, num_samples)
    , m_source_node(std::move(source))
    , m_clear_before_process(clear_before_process)
{
}

void NodeBuffer::setup_processors(ProcessingToken /*token*/)
{
    m_default_processor = create_default_processor();
    m_default_processor->on_attach(shared_from_this());
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
