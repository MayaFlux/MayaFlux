#include "BufferInputControl.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Buffers/Input/InputAudioBuffer.hpp"
#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

// ============================================================================
// Input Buffer Lifecycle
// ============================================================================

void BufferInputControl::setup_audio_input_buffers(uint32_t num_channels, uint32_t buffer_size)
{
    m_audio_input_buffers.clear();

    for (uint32_t i = 0; i < num_channels; ++i) {
        auto input = std::make_shared<InputAudioBuffer>(i, buffer_size);
        auto processor = std::make_shared<InputAccessProcessor>();
        input->set_default_processor(processor);
        m_audio_input_buffers.push_back(input);
    }
}

uint32_t BufferInputControl::get_audio_input_channel_count() const
{
    return static_cast<uint32_t>(m_audio_input_buffers.size());
}

// ============================================================================
// Input Data Processing
// ============================================================================

void BufferInputControl::process_audio_input(double* input_data, uint32_t num_channels, uint32_t num_frames)
{
    if (m_audio_input_buffers.empty() || m_audio_input_buffers.size() < num_channels) {
        setup_audio_input_buffers(num_channels, num_frames);
    }

    if (!input_data) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferInputControl: Invalid input data pointer");
        return;
    }

    for (uint32_t i = 0; i < m_audio_input_buffers.size(); ++i) {
        auto& data = m_audio_input_buffers[i]->get_data();

        for (uint32_t frame = 0; frame < data.size(); ++frame) {
            data[frame] = static_cast<double*>(input_data)[frame * num_channels + i];
        }
        m_audio_input_buffers[i]->process_default();
    }
}

// ============================================================================
// Listener Management
// ============================================================================

void BufferInputControl::register_audio_input_listener(
    const std::shared_ptr<AudioBuffer>& buffer,
    uint32_t channel)
{
    if (channel >= m_audio_input_buffers.size()) {
        MF_ERROR(Journal::Component::Core, Journal::Context::BufferManagement,
            "BufferInputControl: Input channel {} out of range", channel);
        return;
    }

    auto input_buffer = m_audio_input_buffers[channel];
    if (input_buffer) {
        input_buffer->register_listener(buffer);
    }
}

void BufferInputControl::unregister_audio_input_listener(
    const std::shared_ptr<AudioBuffer>& buffer,
    uint32_t channel)
{
    if (channel >= m_audio_input_buffers.size()) {
        return;
    }

    auto input_buffer = m_audio_input_buffers[channel];
    if (input_buffer) {
        input_buffer->unregister_listener(buffer);
    }
}

} // namespace MayaFlux::Buffers
