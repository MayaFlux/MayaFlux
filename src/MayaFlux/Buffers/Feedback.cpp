#include "Feedback.hpp"

namespace MayaFlux::Buffers {

FeedbackBuffer::FeedbackBuffer(u_int32_t channel_id, u_int32_t num_samples, float feedback)
    : StandardAudioBuffer(channel_id, num_samples)
    , m_feedback_amount(feedback)
{
    m_default_processor = create_default_processor();
    m_previous_buffer.resize(num_samples, 0.f);
}

void FeedbackBuffer::process_default()
{
    m_default_processor->process(shared_from_this());
}

void FeedbackProcessor::on_detach(std::shared_ptr<AudioBuffer> buffer)
{
    if (m_using_internal_buffer) {
        m_previous_buffer.clear();
    }
}

std::shared_ptr<BufferProcessor> FeedbackBuffer::create_default_processor()
{
    return std::make_shared<FeedbackProcessor>(m_feedback_amount);
}

FeedbackProcessor::FeedbackProcessor(float feedback)
    : m_feedback_amount(feedback)
    , m_using_internal_buffer(false)
{
}

void FeedbackProcessor::process(std::shared_ptr<AudioBuffer> buffer)
{
    std::vector<double>* previous_data = nullptr;
    std::vector<double> current_data = buffer->get_data();

    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        previous_data = &feedback_buffer->get_previous_buffer();
        m_using_internal_buffer = false;
    } else {
        if (m_previous_buffer.size() != current_data.size()) {
            m_previous_buffer.resize(current_data.size(), 0.0);
            m_using_internal_buffer = true;
            return;
        }
        previous_data = &m_previous_buffer;
    }

    for (size_t i = 0; i < buffer->get_num_samples(); i++) {
        buffer->get_sample(i) += (m_feedback_amount * (*previous_data)[i]);
    }

    *previous_data = current_data;
}

void FeedbackProcessor::on_attach(std::shared_ptr<AudioBuffer> buffer)
{
    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        m_using_internal_buffer = false;

        if (m_feedback_amount != feedback_buffer->get_feedback()) {
            feedback_buffer->set_feedback(m_feedback_amount);
        }
    } else {
        m_previous_buffer.resize(buffer->get_num_samples(), 0.0);
        m_using_internal_buffer = true;
    }
}

}
