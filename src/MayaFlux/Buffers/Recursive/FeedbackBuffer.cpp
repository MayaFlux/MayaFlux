#include "FeedbackBuffer.hpp"

namespace MayaFlux::Buffers {

FeedbackBuffer::FeedbackBuffer(u_int32_t channel_id, u_int32_t num_samples, float feedback)
    : AudioBuffer(channel_id, num_samples)
    , m_feedback_amount(feedback)
{
    m_default_processor = create_default_processor();
    m_previous_buffer.resize(num_samples, 0.f);
}

void FeedbackBuffer::process_default()
{
    m_default_processor->process(shared_from_this());
}

void FeedbackProcessor::on_detach(std::shared_ptr<Buffer> buffer)
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

void FeedbackProcessor::processing_function(std::shared_ptr<Buffer> buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer)
        return;

    std::vector<double>* previous_data = nullptr;
    std::vector<double> current_data = audio_buffer->get_data();
    const size_t num_samples = current_data.size();

    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        previous_data = &feedback_buffer->get_previous_buffer();
        m_using_internal_buffer = false;
    } else {
        if (m_previous_buffer.size() != num_samples) {
            m_previous_buffer.resize(num_samples, 0.0);
            m_using_internal_buffer = true;
        }
        previous_data = &m_previous_buffer;
    }

    auto& buffer_data = audio_buffer->get_data();

    std::ranges::transform(buffer_data, *previous_data, buffer_data.begin(),
        [&feedback = m_feedback_amount](double current, double previous) {
            return current + (feedback * previous);
        });

    *previous_data = std::move(current_data);
}

void FeedbackProcessor::on_attach(std::shared_ptr<Buffer> buffer)
{
    if (auto feedback_buffer = std::dynamic_pointer_cast<FeedbackBuffer>(buffer)) {
        m_using_internal_buffer = false;

        if (m_feedback_amount != feedback_buffer->get_feedback()) {
            feedback_buffer->set_feedback(m_feedback_amount);
        }
    } else {
        m_previous_buffer.resize(std::dynamic_pointer_cast<AudioBuffer>(buffer)->get_num_samples(), 0.0);
        m_using_internal_buffer = true;
    }
}

}
