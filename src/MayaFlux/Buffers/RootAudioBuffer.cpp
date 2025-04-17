#include "RootAudioBuffer.hpp"

namespace MayaFlux::Buffers {

ChannelProcessor::ChannelProcessor(RootAudioBuffer* root_buffer)
    : m_root_buffer(root_buffer)
{
}

void ChannelProcessor::process(std::shared_ptr<AudioBuffer> buffer)
{
    if (!m_root_buffer || buffer.get() != m_root_buffer)
        return;

    auto& root_data = buffer->get_data();
    std::fill(root_data.begin(), root_data.end(), 0.0);

    if (m_root_buffer->has_node_output()) {
        const auto& node_data = m_root_buffer->get_node_output();
        for (size_t i = 0; i < std::min(node_data.size(), root_data.size()); i++) {
            root_data[i] = node_data[i];
        }
    }

    for (auto& child : m_root_buffer->get_child_buffers()) {
        const auto& child_data = child->get_data();
        for (size_t i = 0; i < std::min(child_data.size(), root_data.size()); i++) {
            root_data[i] += child_data[i];
            root_data[i] /= m_root_buffer->get_num_children();
        }
    }

    const double ceiling = 1.0;
    const double softKnee = 0.9;

    for (double& sample : root_data) {
        double abs_sample = std::abs(sample);

        if (abs_sample > softKnee) {
            double excess = abs_sample - softKnee;
            double compression = 1.0 - (excess / (ceiling - softKnee));
            sample *= compression;
        }

        sample = std::clamp(sample, -ceiling, ceiling);
    }
}

RootAudioBuffer::RootAudioBuffer(u_int32_t channel_id, u_int32_t num_samples)
    : StandardAudioBuffer(channel_id, num_samples)
    , m_has_node_output(false)
{
    m_default_processor = create_default_processor();
}

void RootAudioBuffer::set_node_output(const std::vector<double>& data)
{
    if (m_node_output.size() != data.size()) {
        m_node_output.resize(data.size());
    }
    std::copy(data.begin(), data.end(), m_node_output.begin());
    m_has_node_output = true;
}

void RootAudioBuffer::add_child_buffer(std::shared_ptr<AudioBuffer> buffer)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (buffer->get_num_samples() != get_num_samples()) {
        buffer->resize(get_num_samples());
    }

    if (!buffer->get_processing_chain() && get_processing_chain()) {
        buffer->set_processing_chain(get_processing_chain());
    }

    m_child_buffers.push_back(buffer);
}

void RootAudioBuffer::remove_child_buffer(std::shared_ptr<AudioBuffer> buffer)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find(m_child_buffers.begin(), m_child_buffers.end(), buffer);
    if (it != m_child_buffers.end()) {
        m_child_buffers.erase(it);
    }
}

const std::vector<std::shared_ptr<AudioBuffer>>& RootAudioBuffer::get_child_buffers() const
{
    return m_child_buffers;
}

void RootAudioBuffer::process_default()
{
    if (m_default_processor) {
        m_default_processor->process(shared_from_this());
    }
}

void RootAudioBuffer::clear()
{
    StandardAudioBuffer::clear();

    for (auto& child : m_child_buffers) {
        child->clear();
    }
}

void RootAudioBuffer::resize(u_int32_t num_samples)
{
    StandardAudioBuffer::resize(num_samples);

    for (auto& child : m_child_buffers) {
        child->resize(num_samples);
    }
}

std::shared_ptr<BufferProcessor> RootAudioBuffer::create_default_processor()
{
    return std::make_shared<ChannelProcessor>(this);
}
}
