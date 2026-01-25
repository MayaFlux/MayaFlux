#include "FeedbackBuffer.hpp"

namespace MayaFlux::Buffers {

FeedbackBuffer::FeedbackBuffer(uint32_t channel_id, uint32_t num_samples, float feedback, uint32_t feed_samples)
    : AudioBuffer(channel_id, num_samples)
    , m_feedback_amount(feedback)
    , m_feed_samples(feed_samples)
{
    m_default_processor = create_default_processor();
    m_previous_buffer.resize(feed_samples, 0.F);
}

void FeedbackBuffer::process_default()
{
    m_default_processor->process(shared_from_this());
}

void FeedbackProcessor::on_detach(const std::shared_ptr<Buffer>& /*buffer*/)
{
    if (m_using_internal_buffer) {
        m_previous_buffer.clear();
    }
}

std::shared_ptr<BufferProcessor> FeedbackBuffer::create_default_processor()
{
    return std::make_shared<FeedbackProcessor>(m_feedback_amount, m_feed_samples);
}

FeedbackProcessor::FeedbackProcessor(float feedback, uint32_t feed_samples)
    : m_feedback_amount(feedback)
    , m_feed_samples(feed_samples)
    , m_using_internal_buffer(false)
{
}

void FeedbackProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer)
        return;

    std::vector<double>* previous_data = nullptr;
    auto& buffer_data = audio_buffer->get_data();

    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        previous_data = &feedback_buffer->get_previous_buffer();
        m_using_internal_buffer = false;
    } else {
        if (m_previous_buffer.size() != m_feed_samples) {
            m_previous_buffer.resize(m_feed_samples, 0.0);
            m_using_internal_buffer = true;
        }
        previous_data = &m_previous_buffer;
    }

    for (double& sample : buffer_data) {
        double delayed_sample = (*previous_data)[m_buffer_index];

        double output_sample = sample + (m_feedback_amount * delayed_sample);

        if (m_feed_samples > 1) {
            size_t next_index = (m_buffer_index + 1) % m_feed_samples;
            output_sample = (output_sample + (*previous_data)[next_index]) * 0.5;
        }

        sample = output_sample;

        (*previous_data)[m_buffer_index] = output_sample;

        m_buffer_index = (m_buffer_index + 1) % m_feed_samples;
    }
}

void FeedbackProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        m_using_internal_buffer = false;

        if (m_feed_samples != feedback_buffer->get_feed_samples()) {
            m_feed_samples = feedback_buffer->get_feed_samples();
        }

        if (m_feedback_amount != feedback_buffer->get_feedback()) {
            feedback_buffer->set_feedback(m_feedback_amount);
        }
        if (m_previous_buffer.size() != m_feed_samples) {
            m_previous_buffer.resize(m_feed_samples, 0.0);
        }

    } else {
        m_previous_buffer.resize(std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_num_samples(), 0.0);
        m_using_internal_buffer = true;
    }
    m_buffer_index = 0;
}

}
