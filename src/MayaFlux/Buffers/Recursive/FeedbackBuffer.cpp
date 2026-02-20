#include "FeedbackBuffer.hpp"

namespace MayaFlux::Buffers {

//-----------------------------------------------------------------------------
// FeedbackBuffer
//-----------------------------------------------------------------------------

FeedbackBuffer::FeedbackBuffer(uint32_t channel_id, uint32_t num_samples,
    float feedback, uint32_t feed_samples)
    : AudioBuffer(channel_id, num_samples)
    , m_feedback_amount(feedback)
    , m_feed_samples(feed_samples)
    , m_history(feed_samples)
{
    m_default_processor = create_default_processor();
}

void FeedbackBuffer::process_default()
{
    m_default_processor->process(shared_from_this());
}

void FeedbackBuffer::set_feedback(float amount)
{
    m_feedback_amount = amount;
    if (auto proc = std::dynamic_pointer_cast<FeedbackProcessor>(m_default_processor)) {
        proc->set_feedback(amount);
    }
}

void FeedbackBuffer::set_feed_samples(uint32_t samples)
{
    m_feed_samples = samples;
    m_history = Memory::HistoryBuffer<double>(samples);
    if (auto proc = std::dynamic_pointer_cast<FeedbackProcessor>(m_default_processor)) {
        proc->set_feed_samples(samples);
    }
}

std::shared_ptr<BufferProcessor> FeedbackBuffer::create_default_processor()
{
    return std::make_shared<FeedbackProcessor>(m_feedback_amount, m_feed_samples);
}

//-----------------------------------------------------------------------------
// FeedbackProcessor
//-----------------------------------------------------------------------------

FeedbackProcessor::FeedbackProcessor(float feedback, uint32_t feed_samples)
    : m_feedback_amount(feedback)
    , m_feed_samples(feed_samples)
    , m_history(feed_samples)
{
}

void FeedbackProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (auto fb = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        m_active_history = &fb->get_history_buffer();

        if (m_feed_samples != fb->get_feed_samples()) {
            m_feed_samples = fb->get_feed_samples();
        }

        if (m_feedback_amount != fb->get_feedback()) {
            fb->set_feedback(m_feedback_amount);
        }
    } else {
        m_history = Memory::HistoryBuffer<double>(m_feed_samples);
        m_active_history = &m_history;
    }
}

void FeedbackProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    m_active_history = nullptr;
}

void FeedbackProcessor::set_feed_samples(uint32_t samples)
{
    m_feed_samples = samples;
    m_history = Memory::HistoryBuffer<double>(samples);

    if (m_active_history == &m_history) {
        m_active_history = &m_history;
    }
}

void FeedbackProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer) {
        return;
    }

    if (!m_active_history) {
        if (auto fb = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
            m_active_history = &fb->get_history_buffer();
        } else {
            m_active_history = &m_history;
        }
    }

    auto& data = audio_buffer->get_data();

    for (double& sample : data) {
        double delayed = (*m_active_history)[m_feed_samples - 1];

        double output = sample + (m_feedback_amount * delayed);

        m_active_history->push(output);

        sample = output;
    }
}

} // namespace MayaFlux::Buffers
