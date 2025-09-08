#include "DynamicSoundStream.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {

DynamicSoundStream::DynamicSoundStream(u_int32_t sample_rate, u_int32_t num_channels)
    : SoundStreamContainer(sample_rate, num_channels)
    , m_auto_resize(true)
{
}

u_int64_t DynamicSoundStream::validate(std::vector<std::span<const double>>& data, u_int64_t start_frame)
{
    if (data.empty() || data[0].empty()) {
        return 0;
    }

    u_int64_t num_frames {};
    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        num_frames = data[0].size() / get_num_channels();

    } else {
        if (data.size() < get_num_channels()) {
            std::cerr << "Insufficient channel data for planar organization" << '\n';
            return 0;
        }
        num_frames = data[0].size();

        if (!std::ranges::all_of(data | std::views::drop(1) | std::views::take(get_num_channels() - 1),
                [num_frames](const auto& span) { return span.size() == num_frames; })) {
            std::cerr << "Mismatched frame counts across channels" << '\n';
            return 0;
        }
    }

    if (num_frames == 0) {
        std::cerr << "Attempting to write to container with insufficient data for complete frame. Returning" << '\n';
        return 0;
    }

    if (u_int64_t required_end_frame = start_frame + num_frames; m_auto_resize) {
        if (required_end_frame > get_num_frames()) {
            expand_to(required_end_frame);
        }
    } else {
        u_int64_t available_frames = (start_frame < get_num_frames()) ? (get_num_frames() - start_frame) : 0;

        if (available_frames == 0) {
            return 0;
        }

        if (num_frames > available_frames) {
            num_frames = available_frames;
        }

        if (required_end_frame > m_num_frames) {
            m_num_frames = required_end_frame;
            setup_dimensions();
        }
    }
    return num_frames;
}

u_int64_t DynamicSoundStream::validate_single_channel(std::span<const double> data, u_int64_t start_frame, u_int32_t channel)
{
    if (data.empty()) {
        return 0;
    }

    if (channel >= get_num_channels()) {
        std::cerr << "Channel index " << channel << " exceeds available channels (" << get_num_channels() << ")" << '\n';
        return 0;
    }

    u_int64_t num_frames = data.size();
    u_int64_t required_end_frame = start_frame + num_frames;

    if (m_auto_resize) {
        if (required_end_frame > get_num_frames()) {
            expand_to(required_end_frame);
        }
    } else {
        u_int64_t available_frames = (start_frame < get_num_frames()) ? (get_num_frames() - start_frame) : 0;

        if (available_frames == 0) {
            return 0;
        }

        if (num_frames > available_frames) {
            num_frames = available_frames;
        }

        if (required_end_frame > m_num_frames) {
            m_num_frames = required_end_frame;
            setup_dimensions();
        }
    }

    return num_frames;
}

u_int64_t DynamicSoundStream::write_frames(std::vector<std::span<const double>> data, u_int64_t start_frame)
{
    auto num_frames = validate(data, start_frame);

    if (!num_frames)
        return 0;

    if (m_is_circular && start_frame + num_frames > m_circular_capacity) {
        u_int64_t frames_to_end = m_circular_capacity - start_frame;
        u_int64_t frames_from_start = num_frames - frames_to_end;

        if (frames_to_end > 0) {
            std::vector<std::span<const double>> first_part;
            for (const auto& span : data) {
                first_part.emplace_back(span.subspan(0, frames_to_end));
            }
            write_frames(first_part, start_frame);
        }

        if (frames_from_start > 0) {
            std::vector<std::span<const double>> second_part;
            for (const auto& span : data) {
                second_part.emplace_back(span.subspan(frames_to_end, frames_from_start));
            }
            write_frames(second_part, 0);
        }

        return num_frames;
    }

    Region write_region {
        { start_frame, 0 },
        { start_frame + num_frames - 1, get_num_channels() - 1 }
    };

    std::vector<DataVariant> data_variants;

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        u_int64_t samples_to_write = num_frames * get_num_channels();
        data_variants.emplace_back(
            std::vector<double>(data[0].begin(), data[0].begin() + samples_to_write));
    } else {
        data_variants = data
            | std::views::take(get_num_channels())
            | std::views::transform([num_frames](const auto& span) -> DataVariant {
                  return DataVariant(std::vector<double>(span.begin(), span.begin() + num_frames));
              })
            | std::ranges::to<std::vector>();
    }

    set_region_data(write_region, data_variants);

    if (m_is_circular) {
        m_num_frames = std::min(m_circular_capacity, m_num_frames);
    }

    return num_frames;
}

u_int64_t DynamicSoundStream::write_frames(std::span<const double> data, u_int64_t start_frame, u_int32_t channel)
{
    auto num_frames = validate_single_channel(data, start_frame, channel);

    if (!num_frames)
        return 0;

    if (m_is_circular && start_frame + num_frames > m_circular_capacity) {
        u_int64_t frames_to_end = m_circular_capacity - start_frame;
        u_int64_t frames_from_start = num_frames - frames_to_end;

        if (frames_to_end > 0) {
            write_frames(data.subspan(0, frames_to_end), start_frame, channel);
        }

        if (frames_from_start > 0) {
            write_frames(data.subspan(frames_to_end, frames_from_start), 0, channel);
        }

        return num_frames;
    }

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        std::unique_lock lock(m_data_mutex);

        if (m_data.empty()) {
            expand_to(start_frame + num_frames);
        }

        auto& interleaved_data = std::get<std::vector<double>>(m_data[0]);
        u_int32_t num_channels = get_num_channels();

        for (u_int64_t frame = 0; frame < num_frames; ++frame) {
            u_int64_t interleaved_index = (start_frame + frame) * num_channels + channel;
            if (interleaved_index < interleaved_data.size()) {
                interleaved_data[interleaved_index] = data[frame];
            }
        }
    } else {
        std::unique_lock lock(m_data_mutex);

        if (channel >= m_data.size()) {
            return 0;
        }

        auto dest_span = convert_variant<double>(m_data[channel]);

        if (start_frame + num_frames > dest_span.size()) {
            std::vector<double> current_data(dest_span.begin(), dest_span.end());
            current_data.resize(start_frame + num_frames, 0.0);
            m_data[channel] = DataVariant(std::move(current_data));
            dest_span = convert_variant<double>(m_data[channel]);
        }

        std::copy(data.begin(), data.begin() + num_frames,
            dest_span.begin() + start_frame);
    }

    if (m_is_circular) {
        m_num_frames = std::min(m_circular_capacity, m_num_frames);
    }

    invalidate_span_cache();
    m_double_extraction_dirty.store(true, std::memory_order_release);

    return num_frames;
}

std::span<const double> DynamicSoundStream::get_channel_frames(u_int32_t channel, u_int64_t start_frame, u_int64_t num_frames) const
{
    if (channel >= get_num_channels()) {
        return {};
    }

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        std::cerr << "Direct span access not supported for interleaved data. Use get_frames() or Kakshya::extract_channel_data() instead." << '\n';
        return {};
    }

    std::shared_lock lock(m_data_mutex);

    if (channel >= m_data.size()) {
        return {};
    }

    const auto& channel_data = std::get<std::vector<double>>(m_data[channel]);

    if (start_frame >= channel_data.size()) {
        return {};
    }

    u_int64_t available_frames = channel_data.size() - start_frame;
    u_int64_t actual_frames = std::min(num_frames, available_frames);

    return { channel_data.data() + start_frame, actual_frames };
}

void DynamicSoundStream::get_channel_frames(std::span<double> output, u_int32_t channel, u_int64_t start_frame) const
{
    if (channel >= get_num_channels() || output.empty()) {
        return;
    }

    u_int64_t num_frames = output.size();

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        std::shared_lock lock(m_data_mutex);

        if (m_data.empty()) {
            std::ranges::fill(output, 0.0);
            return;
        }

        const auto& interleaved_data = std::get<std::vector<double>>(m_data[0]);
        u_int32_t num_channels = get_num_channels();

        for (u_int64_t frame = 0; frame < num_frames; ++frame) {
            u_int64_t interleaved_index = (start_frame + frame) * num_channels + channel;
            if (interleaved_index < interleaved_data.size()) {
                output[frame] = interleaved_data[interleaved_index];
            } else {
                output[frame] = 0.0;
            }
        }
    } else {
        std::shared_lock lock(m_data_mutex);

        if (channel >= m_data.size()) {
            std::ranges::fill(output, 0.0);
            return;
        }

        const auto& channel_data = std::get<std::vector<double>>(m_data[channel]);

        for (u_int64_t frame = 0; frame < num_frames; ++frame) {
            u_int64_t data_index = start_frame + frame;
            if (data_index < channel_data.size()) {
                output[frame] = channel_data[data_index];
            } else {
                output[frame] = 0.0;
            }
        }
    }
}

void DynamicSoundStream::ensure_capacity(u_int64_t required_frames)
{
    if (u_int64_t current_frames = get_total_elements() / get_num_channels();
        required_frames > current_frames) {
        expand_to(required_frames);
    }
}

void DynamicSoundStream::enable_circular_buffer(u_int64_t capacity)
{
    ensure_capacity(capacity);

    Region circular_region {
        { 0, 0 },
        { capacity - 1, get_num_channels() - 1 }
    };

    set_loop_region(circular_region);
    set_looping(true);

    m_circular_capacity = capacity;
    m_is_circular = true;
}

void DynamicSoundStream::disable_circular_buffer()
{
    set_looping(false);
    m_is_circular = false;
    m_circular_capacity = 0;
}

void DynamicSoundStream::set_all_data(const std::vector<DataVariant>& data)
{
    std::unique_lock lock(m_data_mutex);
    m_data.resize(data.size());

    std::ranges::for_each(std::views::zip(data, m_data),
        [](auto&& pair) {
            auto&& [source, dest] = pair;
            safe_copy_data_variant(source, dest);
        });

    m_num_frames = std::visit([](const auto& vec) {
        return static_cast<u_int64_t>(vec.size());
    },
        m_data[0]);

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        m_num_frames = (get_num_channels() > 0) ? m_num_frames / get_num_channels() : 0;
    }

    setup_dimensions();
    update_processing_state(ProcessingState::READY);
}

void DynamicSoundStream::set_all_data(const DataVariant& data)
{
    set_all_data({ data });
}

void DynamicSoundStream::expand_to(u_int64_t target_frames)
{
    u_int64_t current_frames = get_total_elements() / get_num_channels();
    u_int64_t new_capacity = std::max(target_frames, current_frames * 2);

    std::vector<DataVariant> new_data = create_expanded_data(new_capacity);
    set_all_data(new_data);
}

std::vector<DataVariant> DynamicSoundStream::create_expanded_data(u_int64_t new_frame_count)
{
    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        std::vector<DataVariant> expanded_data(1);

        if (m_data.empty()) {
            expanded_data[0] = DataVariant(std::vector<double>(new_frame_count * get_num_channels(), 0.0));
        } else {
            std::vector<double> current_data;
            extract_from_variant(m_data[0], current_data);

            std::vector<double> expanded_buffer(new_frame_count * get_num_channels(), 0.0);

            std::ranges::copy_n(current_data.begin(),
                std::min(current_data.size(), expanded_buffer.size()),
                expanded_buffer.begin());

            expanded_data[0] = DataVariant(std::move(expanded_buffer));
        }
        return expanded_data;
    }

    return std::views::iota(0U, get_num_channels())
        | std::views::transform([this, new_frame_count](u_int32_t ch) -> DataVariant {
              if (ch < m_data.size()) {
                  std::vector<double> current_channel_data;
                  extract_from_variant(m_data[ch], current_channel_data);

                  std::vector<double> expanded_channel(new_frame_count, 0.0);
                  std::ranges::copy_n(current_channel_data.begin(),
                      std::min(current_channel_data.size(), expanded_channel.size()),
                      expanded_channel.begin());

                  return { std::move(expanded_channel) };
              }

              return { std::vector<double>(new_frame_count, 0.0) };
          })
        | std::ranges::to<std::vector>();
}

}
