#pragma once

#include "config.h"

namespace MayaFlux::Core {

struct AudioBuffer {
    u_int32_t channel_id;
    u_int32_t m_num_samples;
    std::vector<double> m_data;

    AudioBuffer() = default;

    AudioBuffer(u_int32_t channel = 0, u_int32_t num_samples = 512);

    void setup(u_int32_t channel = 0, u_int32_t num_samples = 512);

    inline u_int32_t get_channel_id() const { return channel_id; }

    inline void set_channel_id(u_int32_t id) { channel_id = id; }

    inline u_int32_t get_num_samples() const { return m_num_samples; }

    inline void set_num_samples(u_int32_t num_samples)
    {
        m_num_samples = num_samples;
        m_data.resize(num_samples);
    }

    inline std::vector<double>& get_buffer() { return m_data; }

    inline const std::vector<double>& get_buffer() const { return m_data; }

    inline void set_buffer(const std::vector<double>& buffer)
    {
        m_data = buffer;
        m_num_samples = buffer.size();
    }

    inline void resize(u_int32_t num_samples)
    {
        m_num_samples = num_samples;
        m_data.resize(num_samples);
    }

    inline void clear()
    {
        std::fill(m_data.begin(), m_data.end(), 0.0);
    }
};

class BufferManager {
public:
    BufferManager(u_int32_t num_channels, u_int32_t num_frames);

    u_int32_t get_num_channels() const;

    u_int32_t get_num_frames() const;

    AudioBuffer& get_channel(u_int32_t channel_index);

    const AudioBuffer& get_channel(u_int32_t channel_index) const;

    std::vector<double>& get_channel_buffer(u_int32_t channel_index);

    const std::vector<double>& get_channel_buffer(u_int32_t channel_index) const;

    void fill_from_interleaved(const double* interleaved_data, u_int32_t num_frames);

    void fill_interleaved(double* interleaved_data, u_int32_t num_frames) const;

    void process_channels(std::function<void(AudioBuffer&, u_int32_t)> processor);

    void resize(u_int32_t num_frames);

    inline AudioBuffer& get_main_channel() { return m_audio_buffers[0]; }

    inline std::vector<double>& get_main_channel_buffer() { return m_audio_buffers[0].get_buffer(); }

private:
    u_int32_t m_num_channels;
    u_int32_t m_num_frames;
    std::vector<AudioBuffer> m_audio_buffers;
};

}
