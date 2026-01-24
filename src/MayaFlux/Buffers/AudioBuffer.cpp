#include "AudioBuffer.hpp"

#include "BufferProcessingChain.hpp"

#include "MayaFlux/API/Config.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

AudioBuffer::AudioBuffer()
    : AudioBuffer(0, MayaFlux::Config::get_buffer_size())
{
}

AudioBuffer::AudioBuffer(uint32_t channel_id, uint32_t num_samples)
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
        MF_WARN(Journal::Component::Buffers, Journal::Context::Init,
            "AudioBuffer initialized with a non-default number of samples ({}). This may lead to unexpected behavior.",
            num_samples);
    }
    m_data.resize(num_samples);
}

void AudioBuffer::setup(uint32_t channel, uint32_t num_samples)
{
    m_channel_id = channel;
    resize(num_samples);
}

void AudioBuffer::resize(uint32_t num_samples)
{
    m_num_samples = num_samples;
    m_data.resize(num_samples);
}

void AudioBuffer::clear()
{
    std::ranges::fill(m_data, 0.0);
}

void AudioBuffer::set_num_samples(uint32_t num_samples)
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
        error_rethrow(Journal::Component::Buffers, Journal::Context::Init,
            std::source_location::current(),
            "Error setting default processor: {}", e.what());
    }
}

std::shared_ptr<Buffer> AudioBuffer::clone_to(uint8_t dest_desc)
{
    auto buf = clone_to(static_cast<uint32_t>(dest_desc));
    return std::dynamic_pointer_cast<Buffer>(buf);
}

std::shared_ptr<AudioBuffer> AudioBuffer::clone_to(uint32_t channel)
{
    auto buffer = std::make_shared<AudioBuffer>(channel, m_num_samples);
    buffer->get_data() = m_data;
    buffer->set_default_processor(m_default_processor);
    buffer->set_processing_chain(get_processing_chain(), true);

    return buffer;
}

bool AudioBuffer::read_once(const std::shared_ptr<AudioBuffer>& buffer, bool force)
{
    if (buffer && buffer->get_num_samples() == m_num_samples) {
        if (m_is_processing.load() || buffer->is_processing()) {
            MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "read_once: Attempting to read from an audio buffer while it is being processed.");

            if (!force) {
                MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                    "read_once: Skipping read due to ongoing processing.");
                return false;
            }
            MF_WARN(Journal::Component::Buffers, Journal::Context::BufferProcessing,
                "read_once: Forcing read despite ongoing processing. This may lead to data corruption.");
        }
        m_data = buffer->get_data();
        m_has_data = true;
        return true;
    }

    MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
        "read_once: Buffer read failed due to size mismatch or null buffer.");

    return false;
}

void AudioBuffer::set_processing_chain(std::shared_ptr<BufferProcessingChain> chain, bool force)
{
    if (m_processing_chain && !force) {
        m_processing_chain->merge_chain(chain);
        return;
    }
    m_processing_chain = chain;
}

}
