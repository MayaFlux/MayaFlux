#include "BufferManager.hpp"
#include "MayaFlux/Buffers/NodeSource.hpp"

namespace MayaFlux::Core {

class QuickProcess : public Buffers::BufferProcessor {
public:
    QuickProcess(AudioProcessingFunction function)
        : m_function(function)
    {
    }

    virtual void process(std::shared_ptr<Buffers::AudioBuffer> buffer) override
    {
        m_function(buffer);
    }

private:
    AudioProcessingFunction m_function;
};

BufferManager::BufferManager(u_int32_t num_channels, u_int32_t num_frames)
    : m_num_channels(num_channels)
    , m_num_frames(num_frames)
    , m_global_processing_chain(std::make_shared<Buffers::BufferProcessingChain>())
{
    m_audio_buffers.clear();
    m_audio_buffers.reserve(num_channels);

    m_channel_processing_chains.clear();
    m_channel_processing_chains.reserve(num_channels);

    for (u_int32_t i = 0; i < num_channels; i++) {

        auto buffer { std::make_shared<Buffers::StandardAudioBuffer>(i, num_frames) };
        m_audio_buffers.push_back(buffer);

        std::shared_ptr<Buffers::BufferProcessingChain> chain { std::make_shared<Buffers::BufferProcessingChain>() };
        m_channel_processing_chains.push_back(chain);
    }
}

std::shared_ptr<Buffers::AudioBuffer> BufferManager::get_channel(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index];
}

const std::shared_ptr<Buffers::AudioBuffer> BufferManager::get_channel(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index];
}

std::vector<double>& BufferManager::get_channel_data(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index]->get_data();
}

const std::vector<double>& BufferManager::get_channel_data(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index]->get_data();
}

void BufferManager::process_channel(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto buffer = m_audio_buffers[channel_index];

    m_channel_processing_chains[channel_index]->process(buffer);

    m_global_processing_chain->process(buffer);
}

void BufferManager::process_all_channels()
{
    for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
        process_channel(channel);
    }
}

std::shared_ptr<Buffers::BufferProcessingChain> BufferManager::get_channel_processing_chain(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_channel_processing_chains[channel_index];
}

void BufferManager::add_processor(std::shared_ptr<Buffers::BufferProcessor> processor, u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto buffer = m_audio_buffers[channel_index];
    std::shared_ptr<Buffers::BufferProcessingChain> chain = m_channel_processing_chains[channel_index];

    chain->add_processor(processor, buffer);
}

void BufferManager::add_processor_to_all(std::shared_ptr<Buffers::BufferProcessor> processor)
{
    for (u_int32_t i = 0; i < m_num_channels; i++) {
        auto buffer = m_audio_buffers[i];
        m_global_processing_chain->add_processor(processor, buffer);
    }
}

void BufferManager::remove_processor(std::shared_ptr<Buffers::BufferProcessor> processor, u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }

    auto buffer = m_audio_buffers[channel_index];
    std::shared_ptr<Buffers::BufferProcessingChain> chain = m_channel_processing_chains[channel_index];

    chain->remove_processor(processor, buffer);
}

void BufferManager::remove_processor_from_all(std::shared_ptr<Buffers::BufferProcessor> processor)
{
    for (u_int32_t i = 0; i < m_num_channels; i++) {
        auto buffer = m_audio_buffers[i];
        m_global_processing_chain->remove_processor(processor, buffer);
    }
}

void BufferManager::add_quick_process(AudioProcessingFunction processor, u_int32_t channel_index)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor(quick_process, channel_index);
}

void BufferManager::connect_node_to_channel(std::shared_ptr<Nodes::Node> node, u_int32_t channel_index, float mix)
{
    auto processor = std::make_shared<Buffers::NodeSourceProcessor>(node, mix);
    add_processor(processor, channel_index);
}

void BufferManager::add_quick_processor_to_all(AudioProcessingFunction processor)
{
    auto quick_process = std::make_shared<QuickProcess>(processor);
    add_processor_to_all(quick_process);
}

void BufferManager::fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames)
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            m_audio_buffers[channel]->get_data()[frame] = interleaved_data[frame * m_num_channels + channel];
        }
    }
}

void BufferManager::fill_interleaved(double* interleaved_data, u_int32_t num_frames) const
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            interleaved_data[frame * m_num_channels + channel] = m_audio_buffers[channel]->get_data()[frame];
        }
    }
}

void BufferManager::resize(u_int32_t num_frames)
{
    m_num_frames = num_frames;
    for (auto& buffer : m_audio_buffers) {
        buffer->resize(num_frames);
    }
}

}
