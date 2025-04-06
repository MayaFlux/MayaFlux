#include "Buffer.hpp"

namespace MayaFlux::Core {

AudioBuffer::AudioBuffer(u_int32_t channel, u_int32_t num_samples)
    : channel_id(channel)
    , m_num_samples(num_samples)
{
    m_data.resize(m_num_samples);
}

void AudioBuffer::setup(u_int32_t channel, u_int32_t num_samples)
{
    channel_id = channel;
    m_num_samples = num_samples;
    m_data.resize(m_num_samples);
}

BufferManager::BufferManager(u_int32_t num_channels, u_int32_t num_frames)
    : m_num_channels(num_channels)
    , m_num_frames(num_frames)
{
    m_audio_buffers.clear();
    m_audio_buffers.reserve(num_channels);

    for (u_int32_t i = 0; i < num_channels; i++) {
        m_audio_buffers.emplace_back(i, num_frames);
    }
}

u_int32_t BufferManager::get_num_channels() const
{
    return m_num_channels;
}

u_int32_t BufferManager::get_num_frames() const
{
    return m_num_frames;
}

AudioBuffer& BufferManager::get_channel(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index];
}

const AudioBuffer& BufferManager::get_channel(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index];
}

std::vector<double>& BufferManager::get_channel_buffer(u_int32_t channel_index)
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index].get_buffer();
}

const std::vector<double>& BufferManager::get_channel_buffer(u_int32_t channel_index) const
{
    if (channel_index >= m_num_channels) {
        throw std::out_of_range("Channel index out of range");
    }
    return m_audio_buffers[channel_index].get_buffer();
}

void BufferManager::fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames)
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            m_audio_buffers[channel].get_buffer()[frame] = interleaved_data[frame * m_num_channels + channel];
        }
    }
}

void BufferManager::fill_interleaved(double* interleaved_data, u_int32_t num_frames) const
{
    num_frames = std::min(num_frames, m_num_frames);

    for (u_int32_t frame = 0; frame < num_frames; frame++) {
        for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
            interleaved_data[frame * m_num_channels + channel] = m_audio_buffers[channel].get_buffer()[frame];
        }
    }
}

void BufferManager::process_channels(std::function<void(AudioBuffer&, u_int32_t)> processor)
{
    for (u_int32_t channel = 0; channel < m_num_channels; channel++) {
        processor(m_audio_buffers[channel], channel);
    }
}

void BufferManager::resize(u_int32_t num_frames)
{
    m_num_frames = num_frames;
    for (auto& buffer : m_audio_buffers) {
        buffer.resize(num_frames);
    }
}

}
