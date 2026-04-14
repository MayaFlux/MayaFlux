#include "CursorAccessProcessor.hpp"

#include "MayaFlux/Kakshya/Source/DynamicSoundStream.hpp"
#include "MayaFlux/Kakshya/Utils/DataUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Kakshya {

CursorAccessProcessor::CursorAccessProcessor(uint64_t frames_per_block)
    : m_frames_per_block(frames_per_block)
{
}

void CursorAccessProcessor::on_attach(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto stream = std::dynamic_pointer_cast<DynamicSoundStream>(container);
    if (!stream) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "CursorAccessProcessor requires a DynamicSoundStream");
        return;
    }

    m_structure = container->get_structure();

    m_slot_index = stream->allocate_dynamic_slot();

    const uint64_t num_channels = m_structure.get_channel_count();
    m_cursor.assign(num_channels, m_loop_start);
}

void CursorAccessProcessor::on_detach(const std::shared_ptr<SignalSourceContainer>& container)
{
    auto stream = std::dynamic_pointer_cast<DynamicSoundStream>(container);
    if (!stream) {
        MF_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "CursorAccessProcessor requires a DynamicSoundStream, slot index can potentially leak");
        return;
    }

    m_active = false;
    m_cursor.clear();
    stream->release_dynamic_slot(m_slot_index);
}

void CursorAccessProcessor::process(const std::shared_ptr<SignalSourceContainer>& container)
{
    m_is_processing.store(true, std::memory_order_relaxed);

    auto stream = std::dynamic_pointer_cast<DynamicSoundStream>(container);
    if (!stream) {
        MF_RT_ERROR(Journal::Component::Kakshya, Journal::Context::ContainerProcessing,
            "CursorAccessProcessor requires a DynamicSoundStream");
        m_is_processing.store(false, std::memory_order_relaxed);
        return;
    }

    auto& pd = stream->get_dynamic_data(m_slot_index);

    if (!m_active) {
        const uint64_t num_channels = m_structure.get_channel_count();
        if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
            pd.resize(1);
            std::get<std::vector<double>>(pd[0]).assign(m_frames_per_block * num_channels, 0.0);
        } else {
            pd.resize(num_channels);
            for (auto& ch : pd)
                std::get<std::vector<double>>(ch).assign(m_frames_per_block, 0.0);
        }
        m_is_processing.store(false, std::memory_order_relaxed);
        return;
    }

    const uint64_t frame = m_cursor.empty() ? 0 : m_cursor[0];

    const uint64_t total_frames = m_structure.get_samples_count_per_channel();
    const uint64_t read_end = std::min(frame + m_frames_per_block - 1, total_frames - 1);

    const Region region {
        std::vector<uint64_t> { frame, 0 },
        std::vector<uint64_t> { read_end, m_structure.get_channel_count() - 1 }
    };

    auto region_data = container->get_region_data(region);

    if (m_structure.organization == OrganizationStrategy::INTERLEAVED) {
        pd.resize(1);
        if (!region_data.empty())
            safe_copy_data_variant(region_data[0], pd[0]);
    } else {
        const uint64_t ch_count = std::min(
            static_cast<uint64_t>(region_data.size()),
            m_structure.get_channel_count());
        pd.resize(ch_count);
        for (size_t c = 0; c < ch_count; ++c)
            safe_copy_data_variant(region_data[c], pd[c]);
    }

    const double raw_advance = static_cast<double>(m_frames_per_block) * m_speed + m_speed_remainder;
    const auto int_advance = static_cast<uint64_t>(raw_advance);
    m_speed_remainder = raw_advance - static_cast<double>(int_advance);

    const uint64_t next = frame + int_advance;
    const uint64_t loop_end = m_loop_end;

    if (next >= loop_end) {
        if (m_looping) {
            if (m_loop_count > 0) {
                --m_loops_remaining;
                if (m_loops_remaining == 0) {
                    m_active = false;
                    m_speed_remainder = 0.0;
                    m_cursor.assign(m_cursor.size(), m_loop_start);
                    if (m_on_end)
                        m_on_end();
                    m_is_processing.store(false, std::memory_order_relaxed);
                    return;
                }
            }
            m_speed_remainder = 0.0;
            m_cursor.assign(m_cursor.size(), m_loop_start);
        } else {
            m_active = false;
            m_speed_remainder = 0.0;
            m_cursor.assign(m_cursor.size(), m_loop_start);
            if (m_on_end)
                m_on_end();
        }
    } else {
        m_cursor.assign(m_cursor.size(), next);
    }

    m_is_processing.store(false, std::memory_order_relaxed);
}

void CursorAccessProcessor::reset()
{
    m_cursor.assign(m_cursor.size(), m_loop_start);
    m_loops_remaining = m_loop_count;
    m_active = true;
}

void CursorAccessProcessor::set_speed(double speed)
{
    if (speed > 0.0)
        m_speed = speed;
}

void CursorAccessProcessor::stop()
{
    m_active = false;
}

void CursorAccessProcessor::set_loop_count(size_t n)
{
    m_loop_count = n;
    m_loops_remaining = n;
}

void CursorAccessProcessor::set_frames_per_block(uint64_t frames_per_block)
{
    m_frames_per_block = frames_per_block;
}

void CursorAccessProcessor::set_loop_region(uint64_t start_frame, uint64_t end_frame)
{
    m_loop_start = start_frame;
    m_loop_end = end_frame;
}

} // namespace MayaFlux::Kakshya
