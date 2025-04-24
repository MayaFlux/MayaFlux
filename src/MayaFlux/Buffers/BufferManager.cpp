#include "BufferManager.hpp"
#include "Node/NodeBuffer.hpp"

namespace MayaFlux::Buffers {

class QuickProcess : public BufferProcessor {
public:
    QuickProcess(AudioProcessingFunction function)
        : m_function(function)
    {
    }

    virtual void process(std::shared_ptr<AudioBuffer> buffer) override
    {
        m_function(buffer);
    }

private:
    AudioProcessingFunction m_function;
};

BufferManager::BufferManager(u_int32_t num_channels, u_int32_t num_frames)
    : m_num_channels(num_channels)
    , m_num_frames(num_frames)
    , m_global_processing_chain(std::make_shared<BufferProcessingChain>())
{
    m_root_buffers.clear();
    m_root_buffers.reserve(num_channels);

    m_channel_processing_chains.clear();
    m_channel_processing_chains.reserve(num_channels);

    for (u_int32_t i = 0; i < num_channels; i++) {

        auto buffer { std::make_shared<RootAudioBuffer>(i, num_frames) };
        m_root_buffers.push_back(buffer);

        std::shared_ptr<BufferProcessingChain> chain { std::make_shared<BufferProcessingChain>() };
        m_channel_processing_chains.push_back(chain);

        buffer->set_processing_chain(chain);
    }

    auto limiter = std::make_shared<FinalLimiterProcessor>();
    set_final_processor_for_root_buffers(limiter);
}

std::shared_ptr<AudioBuffer> BufferManager::get_channel(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_root_buffers[channel_index];
}

const std::shared_ptr<AudioBuffer> BufferManager::get_channel(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_root_buffers[channel_index];
}

std::vector<double>& BufferManager::get_channel_data(u_int32_t channel_index)
{
    return get_channel(channel_index)->get_data();
}

const std::vector<double>& BufferManager::get_channel_data(u_int32_t channel_index) const
{
    return get_channel(channel_index)->get_data();
}

void BufferManager::add_buffer_to_channel(u_int32_t channel_index, std::shared_ptr<AudioBuffer> buffer)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    buffer->set_channel_id(channel_index);

    if (auto buf_chain = buffer->get_processing_chain()) {
        if (buf_chain != m_channel_processing_chains[channel_index]) {
            m_channel_processing_chains[channel_index]->merge_chain(buf_chain);
        }
    } else {
        buffer->set_processing_chain(m_channel_processing_chains[channel_index]);
    }

    m_root_buffers[channel_index]->add_child_buffer(buffer);
}

void BufferManager::remove_buffer_from_channel(u_int32_t channel_index, std::shared_ptr<AudioBuffer> buffer)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    m_root_buffers[channel_index]->remove_child_buffer(buffer);
}

const std::vector<std::shared_ptr<AudioBuffer>>& BufferManager::get_channel_buffers(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    return m_root_buffers[channel_index]->get_child_buffers();
}

void BufferManager::process_channel(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto root_buffer = m_root_buffers[channel_index];

    for (auto& child : root_buffer->get_child_buffers()) {

        if (child->needs_default_processing()) {
            child->process_default();
        }

        if (auto processing_chain = child->get_processing_chain()) {
            if (child->has_data_for_cycle()) {
                processing_chain->process(child);
            }
        }
    }

    root_buffer->process_default();
    m_channel_processing_chains[channel_index]->process(root_buffer);
    m_global_processing_chain->process(root_buffer);

    if (auto chain = root_buffer->get_processing_chain()) {
        chain->process_final(root_buffer);
    }
}

void BufferManager::process_all_channels()
{
    for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
        process_channel(channel);
    }
}

std::shared_ptr<BufferProcessingChain> BufferManager::get_channel_processing_chain(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_channel_processing_chains[channel_index];
}

void BufferManager::add_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    u_int32_t channel_id = buffer->get_channel_id();

    if (channel_id < m_num_channels) {
        std::shared_ptr<BufferProcessingChain> chain = m_channel_processing_chains[channel_id];
        chain->add_processor(processor, buffer);
        buffer->set_processing_chain(chain);
    }
}

void BufferManager::add_processor_to_channel(std::shared_ptr<BufferProcessor> processor, u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto buffer = m_root_buffers[channel_index];
    std::shared_ptr<BufferProcessingChain> chain = m_channel_processing_chains[channel_index];

    chain->add_processor(processor, buffer);
}

void BufferManager::add_processor_to_all(std::shared_ptr<BufferProcessor> processor)
{
    for (u_int32_t i = 0; i < m_num_channels; i++) {
        auto buffer = m_root_buffers[i];
        m_global_processing_chain->add_processor(processor, buffer);
    }
}
void BufferManager::remove_processor(std::shared_ptr<BufferProcessor> processor, std::shared_ptr<AudioBuffer> buffer)
{
    std::shared_ptr<BufferProcessingChain> chain = m_channel_processing_chains[buffer->get_channel_id()];

    chain->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_channel(std::shared_ptr<BufferProcessor> processor, u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto buffer = m_root_buffers[channel_index];
    std::shared_ptr<BufferProcessingChain> chain = m_channel_processing_chains[channel_index];

    chain->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_all(std::shared_ptr<BufferProcessor> processor)
{
    for (u_int32_t i = 0; i < m_num_channels; i++) {
        auto buffer = m_root_buffers[i];
        m_global_processing_chain->remove_processor(processor, buffer);
    }
}

void BufferManager::set_final_processor_for_root_buffers(std::shared_ptr<BufferProcessor> processor)
{
    for (auto& root_buffer : m_root_buffers) {
        if (auto chain = root_buffer->get_processing_chain()) {
            chain->add_final_processor(processor, root_buffer);
        }
    }
}

void BufferManager::connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix, bool clear_before)
{
    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);
    add_processor_to_channel(processor, channel_index);
}

void BufferManager::connect_node_to_buffer(std::shared_ptr<Nodes::Node> node, std::shared_ptr<AudioBuffer> buffer, float mix, bool clear_before)
{
    auto processor = std::make_shared<NodeSourceProcessor>(node, mix, clear_before);
    add_processor(processor, buffer);
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process(AudioProcessingFunction processor, std::shared_ptr<AudioBuffer> buffer)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor(quick_process, buffer);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process_to_channel(AudioProcessingFunction processor, u_int32_t channel_index)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor_to_channel(quick_process, channel_index);
    return quick_process;
}

std::shared_ptr<BufferProcessor> BufferManager::attach_quick_process_to_all(AudioProcessingFunction processor)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor_to_all(quick_process);
    return quick_process;
}

void BufferManager::fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames)
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            m_root_buffers[channel]->get_data()[frame] = interleaved_data[frame * m_num_channels + channel];
        }
    }
}

void BufferManager::fill_interleaved(double* interleaved_data, u_int32_t num_frames) const
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            interleaved_data[frame * m_num_channels + channel] = m_root_buffers[channel]->get_data()[frame];
        }
    }
}

void BufferManager::resize(u_int32_t num_frames)
{
    m_num_frames = num_frames;
    for (auto& buffer : m_root_buffers) {
        buffer->resize(num_frames);
    }
}

}
