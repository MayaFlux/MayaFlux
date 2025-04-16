#pragma once

#include "AudioBuffer.hpp"
#include "BufferProcessor.hpp"

namespace MayaFlux::Buffers {

class FeedbackBuffer : public StandardAudioBuffer {
public:
    FeedbackBuffer(u_int32_t channel_id = 0, u_int32_t num_samples = 512, float feedback = 0.5f);

    inline void set_feedback(float amount) { m_feedback_amount = amount; }
    inline float get_feedback() const { return m_feedback_amount; }

    inline std::vector<double>& get_previous_buffer() { return m_previous_buffer; }
    inline const std::vector<double>& get_previous_buffer() const { return m_previous_buffer; }

    void process_default() override;

protected:
    std::shared_ptr<BufferProcessor> create_default_processor() override;
    std::shared_ptr<BufferProcessor> m_default_processor;

private:
    float m_feedback_amount;
    std::vector<double> m_previous_buffer;
};

class FeedbackProcessor : public BufferProcessor {

public:
    FeedbackProcessor(float feedback = 0.5f);

    void process(std::shared_ptr<AudioBuffer> buffer) override;

    void on_attach(std::shared_ptr<AudioBuffer> buffer) override;
    void on_detach(std::shared_ptr<AudioBuffer> buffer) override;

    inline void set_feedback(float amount) { m_feedback_amount = amount; }
    inline float get_feedback() const { return m_feedback_amount; }

private:
    float m_feedback_amount;
    std::vector<double> m_previous_buffer;
    bool m_using_internal_buffer;
};
}
