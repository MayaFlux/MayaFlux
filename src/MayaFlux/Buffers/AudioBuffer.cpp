#include "AudioBuffer.hpp"
#include "BufferProcessingChain.hpp"

#include "MayaFlux/API/Config.hpp"

namespace MayaFlux::Buffers {

AudioBuffer::AudioBuffer()
    : AudioBuffer(0, MayaFlux::Config::get_buffer_size())
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
    if (num_samples != MayaFlux::Config::get_buffer_size()) {
        std::cerr << "Warning: AudioBuffer initialized with a non-default number of samples ("
                  << num_samples << "). This may lead to unexpected behavior." << std::endl;
    }
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

std::shared_ptr<AudioBuffer> AudioBuffer::clone_to(u_int32_t channel)
{
    auto buffer = std::make_shared<AudioBuffer>(channel, m_num_samples);
    buffer->get_data() = m_data;
    buffer->set_default_processor(m_default_processor);
    buffer->set_processing_chain(get_processing_chain());

    return buffer;
}

bool AudioBuffer::read_once(std::shared_ptr<AudioBuffer> buffer, bool force)
{
    if (buffer && buffer->get_num_samples() == m_num_samples) {
        if (m_is_processing.load() || buffer->is_processing()) {
            std::cerr << "Warning: Attempting to read from an audio buffer while it is being processed."
                      << std::endl;

            if (!force) {
                std::cerr << "Skipping read due to ongoing processing." << std::endl;
                return false;
            }
            std::cerr << "Copying between buffers that are processing. This can lead to data curroption" << std::endl;
        }
        m_data = buffer->get_data();
        m_has_data = true;
        return true;
    } else {
        std::cerr << "Error: Buffer read failed due to size mismatch or null buffer." << std::endl;
    }
    return false;
}

}
