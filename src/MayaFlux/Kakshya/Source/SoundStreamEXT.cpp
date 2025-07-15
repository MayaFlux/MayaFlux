#include "SoundStreamEXT.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

namespace MayaFlux::Kakshya {
u_int64_t SSCExt::write_frames(std::span<const double> data, u_int64_t start_frame)
{
    u_int64_t num_frames = data.size() / get_num_channels();

    if (m_auto_resize) {
        ensure_capacity(start_frame + num_frames);
    }

    Region write_region;
    write_region.start_coordinates = { start_frame, 0 };
    write_region.end_coordinates = { start_frame + num_frames - 1, get_num_channels() - 1 };

    DataVariant data_variant = std::vector<double>(data.begin(), data.end());
    set_region_data(write_region, data_variant);

    return num_frames;
}
void SSCExt::ensure_capacity(u_int64_t required_frames)
{
    u_int64_t current_frames = get_total_elements() / get_num_channels();
    if (required_frames > current_frames) {
        expand_to(required_frames);
    }
}
void SSCExt::enable_circular_buffer(u_int64_t capacity)
{
    ensure_capacity(capacity);

    Region circular_region;
    circular_region.start_coordinates = { 0, 0 };
    circular_region.end_coordinates = { capacity - 1, get_num_channels() - 1 };

    set_loop_region(circular_region);
    set_looping(true);

    m_circular_capacity = capacity;
    m_is_circular = true;
}

void SSCExt::disable_circular_buffer()
{
    set_looping(false);
    m_is_circular = false;
    m_circular_capacity = 0;
}

void SSCExt::set_all_data(const DataVariant& data)
{
    std::unique_lock lock(m_data_mutex);

    safe_copy_data_variant(data, m_data);

    u_int64_t total_elements = std::visit([](const auto& vec) {
        return static_cast<u_int64_t>(vec.size());
    },
        m_data);

    m_num_frames = (get_num_channels() > 0) ? total_elements / get_num_channels() : 0;

    setup_dimensions();

    update_processing_state(ProcessingState::READY);
}

void SSCExt::expand_to(u_int64_t target_frames)
{
    u_int64_t current_frames = get_total_elements() / get_num_channels();
    u_int64_t new_capacity = std::max(target_frames, current_frames * 2);

    DataVariant new_data = create_expanded_data(new_capacity);
    set_all_data(new_data);
}

DataVariant SSCExt::create_expanded_data(u_int64_t new_frame_count)
{
    auto current_data = get_typed_data<double>(m_data);

    std::vector<double> expanded(new_frame_count * get_num_channels(), 0.0);

    if (!current_data.empty()) {
        std::copy(current_data.begin(), current_data.end(), expanded.begin());
    }

    return expanded;
}

}
