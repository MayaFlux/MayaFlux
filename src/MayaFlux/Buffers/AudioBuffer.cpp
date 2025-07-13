#include "AudioBuffer.hpp"
#include "BufferProcessingChain.hpp"

namespace MayaFlux::Buffers {

AudioBuffer::AudioBuffer()
    : AudioBuffer(0, 512)
{
}

AudioBuffer::AudioBuffer(u_int32_t channel_id, u_int32_t num_samples)
    : m_channel_id(channel_id)
    , m_num_samples(num_samples)
    , m_default_processor(nullptr)
    , m_has_data(true)
    , m_should_remove(false)
    , m_process_default(true)
    , m_processing_chain(std::make_shared<BufferProcessingChain>())
    , m_is_processing(false)
{
    m_data.resize(num_samples);
}

void AudioBuffer::setup(u_int32_t channel, u_int32_t num_samples)
{
    m_channel_id = channel;
    resize(num_samples);
}

void AudioBuffer::resize(u_int32_t num_samples)
{
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void AudioBuffer::clear()
{
    std::fill(m_data.begin(), m_data.end(), 0.0);
}

void AudioBuffer::set_num_samples(u_int32_t num_samples)
{
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void AudioBuffer::process_default()
{
    if (m_process_default && m_default_processor) {
        m_default_processor->process(shared_from_this());
    }
}

void AudioBuffer::set_default_processor(std::shared_ptr<BufferProcessor> processor)
{
    try {
        if (m_default_processor) {
            m_default_processor->on_detach(shared_from_this());
        }
        if (processor) {
            processor->on_attach(shared_from_this());
        }
        m_default_processor = processor;
    } catch (const std::exception& e) {
        std::cout << "Exception in set_default_processor: " << e.what() << std::endl;
        throw;
    }
}
}
