#pragma once

#include "SoundStreamContainer.hpp"

namespace MayaFlux::Kakshya {

/**
 * @class SSCExt
 * @brief Minimal extension for dynamic resizing and buffer-sized operations
 *
 * Adds only what's missing: dynamic capacity management and buffer-sized wrappers.
 * Everything else delegates to existing SoundStreamContainer infrastructure.
 */
class SSCExt : public SoundStreamContainer {
public:
    SSCExt(u_int32_t sample_rate = 48000, u_int32_t num_channels = 2)
        : SoundStreamContainer(sample_rate, num_channels)
        , m_auto_resize(true)
    {
    }

    /**
     * @brief Write buffer data, auto-expanding if needed
     */
    u_int64_t write_frames(std::span<const double> data, u_int64_t start_frame = 0);

    /**
     * @brief Read buffer data using existing sequential read
     */
    inline u_int64_t read_frames(std::span<double> output, u_int64_t count)
    {
        return read_sequential(output, count);
    }

    inline void set_auto_resize(bool enable) { m_auto_resize = enable; }
    inline bool get_auto_resize() const { return m_auto_resize; }

    void ensure_capacity(u_int64_t required_frames);

    void enable_circular_buffer(u_int64_t capacity);

    void disable_circular_buffer();

    bool is_circular() const { return m_is_circular; }

private:
    bool m_auto_resize = true;
    bool m_is_circular = false;
    u_int64_t m_circular_capacity = 0;

    void expand_to(u_int64_t target_frames);

    DataVariant create_expanded_data(u_int64_t new_frame_count);

    void set_all_data(const DataVariant& new_data);
};

} // namespace MayaFlux::Kakshya
