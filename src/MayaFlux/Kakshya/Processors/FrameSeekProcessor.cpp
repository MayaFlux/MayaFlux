#include "FrameSeekProcessor.hpp"

#include "MayaFlux/Kakshya/Source/VideoStreamContainer.hpp"
#include "MayaFlux/Kakshya/Utils/PixelStorage.hpp"

#include "MayaFlux/Kinesis/Scalar.hpp"
#include "MayaFlux/Kinesis/Tendency/TendencyFactories.hpp"
#include "MayaFlux/Kinesis/Tendency/TimeMap.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

void FrameSeekProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    FrameAccessProcessor::on_attach(container);
    reset();
}

void FrameSeekProcessor::set_time_map(const std::shared_ptr<const Kinesis::TimeMap>& map)
{
    if (map == m_time_map) {
        return;
    }

    m_time_map = map;
    reset();
}

void FrameSeekProcessor::set_repeat(const std::shared_ptr<const Kinesis::TimeMap>& length)
{
    if (length == m_repeat) {
        return;
    }

    m_repeat = length;
    m_repeat_start = 0;
    m_repeat_length = 0;
}

void FrameSeekProcessor::trail(double write_head, Kinesis::TimeMap lag)
{
    const double fps = m_global_fps > 0.0 ? m_global_fps : 60.0;

    set_time_map(std::make_shared<const Kinesis::TimeMap>(
        Kinesis::TimeMaps::lagged(write_head, fps, std::move(lag))));
}

void FrameSeekProcessor::trail(double write_head, double lag_frames)
{
    trail(write_head, Kinesis::constant<double, double>(lag_frames));
}

void FrameSeekProcessor::reset()
{
    m_ticks = 0;
    m_repeat_start = 0;
    m_repeat_length = 0;
}

double FrameSeekProcessor::position_at(uint64_t tick)
{
    const double fps = m_global_fps > 0.0 ? m_global_fps : 60.0;
    double seconds = static_cast<double>(tick) / fps;

    if (m_repeat && m_repeat->fn) {
        if (m_repeat_length == 0 || tick - m_repeat_start >= m_repeat_length) {
            m_repeat_start = tick;
            const double length = std::max((*m_repeat)(seconds), 0.0);
            m_repeat_length = std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(length * fps)));
        }

        seconds = static_cast<double>(tick - m_repeat_start) / fps;
    }

    return (*m_time_map)(seconds);
}

void FrameSeekProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto source = m_source_container_weak.lock();
    auto video = std::dynamic_pointer_cast<VideoStreamContainer>(source);

    if (video) {
        m_total_frames = video->get_num_frames();
    }

    if (!m_time_map || !m_time_map->fn) {
        FrameAccessProcessor::process(container);
        return;
    }

    if (!m_prepared) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameSeekProcessor not prepared for processing");
        return;
    }

    if (!video || source.get() != container.get()) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameSeekProcessor: source container mismatch, expired or not a VideoStreamContainer");
        return;
    }

    if (m_total_frames == 0 || m_frame_byte_size == 0) {
        return;
    }

    m_is_processing = true;

    auto lo = 0.0;
    auto hi = static_cast<double>(m_total_frames);

    if (m_looping_enabled) {
        if (!m_loop_region.start_coordinates.empty()) {
            lo = static_cast<double>(m_loop_region.start_coordinates[0]);
        }
        if (!m_loop_region.end_coordinates.empty()) {
            hi = std::min(hi, static_cast<double>(m_loop_region.end_coordinates[0] + 1));
        }
        if (hi <= lo) {
            lo = 0.0;
            hi = static_cast<double>(m_total_frames);
        }
    }

    auto& processed = container->get_processed_data();
    processed.resize(1);

    auto* dest = std::get_if<std::vector<uint8_t>>(&processed[0]);
    if (!dest) {
        processed[0] = std::vector<uint8_t>();
        dest = std::get_if<std::vector<uint8_t>>(&processed[0]);
    }
    dest->resize(m_frames_per_batch * m_frame_byte_size);

    uint8_t* out = dest->data();
    uint64_t last_frame = 0;
    bool all_ok = true;

    for (uint64_t k = 0; k < m_frames_per_batch; ++k, out += m_frame_byte_size) {
        const double position = position_at(m_ticks + k);
        const double s = m_looping_enabled
            ? Kinesis::wrap<double>(position, lo, hi)
            : std::clamp(position, lo, hi - 1.0);
        const double whole = std::floor(s);
        const double weight = s - whole;
        const auto first = static_cast<uint64_t>(whole);
        last_frame = first;

        const auto a = video->get_frame_pixels(first);
        if (a.empty()) {
            std::memset(out, 0, m_frame_byte_size);
            all_ok = false;
            continue;
        }

        if (m_blend && weight > 0.0) {
            auto next = first + 1;
            if (static_cast<double>(next) >= hi) {
                next = m_looping_enabled ? static_cast<uint64_t>(lo) : first;
            }

            const auto b = video->get_frame_pixels(next);
            if (!b.empty() && blend_frames(video->get_format(), a, b, out, weight)) {
                continue;
            }
        }

        std::memcpy(out, a.data(), m_frame_byte_size);
    }

    if (!all_ok) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "FrameSeekProcessor: one or more frames unavailable near frame {}", last_frame);
    }

    m_ticks += m_frames_per_batch;
    m_current_frame = last_frame;
    video->update_read_position_for_channel(0, last_frame);

    m_is_processing = false;
}

} // namespace MayaFlux::Kakshya
