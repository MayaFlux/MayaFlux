#include "AudioBuffer.hpp"
#include "BufferProcessor.hpp"

namespace MayaFlux::Buffers {

StandardAudioBuffer::StandardAudioBuffer()
    : m_channel_id(0)
    , m_num_samples(0)
    , m_default_processor(create_default_processor())
    , m_processing_chain(std::make_shared<BufferProcessingChain>())
{
}

StandardAudioBuffer::StandardAudioBuffer(u_int32_t channel_id, u_int32_t num_samples)
    : m_channel_id(channel_id)
    , m_num_samples(num_samples)
    , m_default_processor(create_default_processor())
    , m_processing_chain(std::make_shared<BufferProcessingChain>())
{
    m_data.resize(num_samples);
}

void StandardAudioBuffer::setup(u_int32_t channel, u_int32_t num_samples)
{
    m_channel_id = channel;
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void StandardAudioBuffer::resize(u_int32_t num_samples)
{
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void StandardAudioBuffer::clear()
{
    std::fill(m_data.begin(), m_data.end(), 0.0);
}

void StandardAudioBuffer::set_num_samples(u_int32_t num_samples)
{
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void StandardAudioBuffer::process_default()
{
    if (m_default_processor) {
        m_default_processor->process(shared_from_this());
    }
}

// TODO: May just be unnecessary with overriding Bufferprocessor
/* void StandardAudioBuffer::process(const std::any& params)
{
    // TODO: If necessary, add std::any-> processor methods before global process
    process();
} */

void StandardAudioBuffer::set_default_processor(std::shared_ptr<BufferProcessor> processor)
{
    if (m_default_processor) {
        m_default_processor->on_detach(shared_from_this());
    }

    m_default_processor = processor;

    if (m_default_processor) {
        m_default_processor->on_attach(shared_from_this());
    }
}

void StandardAudioBuffer::set_processing_chain(std::shared_ptr<BufferProcessingChain> chain)
{
    m_processing_chain = chain;
}
}
