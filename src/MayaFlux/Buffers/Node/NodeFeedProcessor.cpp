#include "NodeFeedProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Nodes/Conduit/StreamReaderNode.hpp"

namespace MayaFlux::Buffers {

NodeFeedProcessor::NodeFeedProcessor(std::shared_ptr<Nodes::StreamReaderNode> target)
    : m_target(std::move(target))
{
}

void NodeFeedProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    if (!m_target) {
        return;
    }

    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer) {
        return;
    }

    const auto& data = audio_buffer->get_data();
    m_target->set_data(data, this);
}

void NodeFeedProcessor::on_attach(const std::shared_ptr<Buffer>& /*buffer*/)
{
}

void NodeFeedProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_target) {
        m_target->release_owner(this);
    }
}

void NodeFeedProcessor::set_target(std::shared_ptr<Nodes::StreamReaderNode> target)
{
    if (m_target) {
        m_target->release_owner(this);
    }
    m_target = std::move(target);
}

}
