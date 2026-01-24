#include "SoundStreamWriter.hpp"
#include "MayaFlux/Buffers/AudioBuffer.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void SoundStreamWriter::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio_buffer = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio_buffer || !m_container)
        return;

    std::span<const double> data_span = audio_buffer->get_data();

    if (data_span.empty())
        return;

    uint32_t channel_id = audio_buffer->get_channel_id();

    if (channel_id >= m_container->get_num_channels()) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "SoundStreamWriter: AudioBuffer channel {} exceeds container channels ({}). Skipping write.",
            channel_id, m_container->get_num_channels());
        return;
    }

    uint64_t frames_written = m_container->write_frames(data_span, m_write_position, channel_id);

    if (frames_written > 0) {
        m_write_position += frames_written;

        if (m_container->is_circular() && m_write_position >= m_container->get_circular_capacity()) {
            m_write_position = 0;
        }
    }
}

void SoundStreamWriter::set_write_position_time(double time_seconds)
{
    if (m_container) {
        m_write_position = m_container->time_to_position(time_seconds);
    }
}

double SoundStreamWriter::get_write_position_time() const
{
    if (m_container) {
        return m_container->position_to_time(m_write_position);
    }
    return 0.0;
}
}
