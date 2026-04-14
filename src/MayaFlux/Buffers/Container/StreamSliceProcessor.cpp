#include "StreamSliceProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kakshya/Utils/ContainerUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

void StreamSliceProcessor::on_attach(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio) {
        MF_RT_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "StreamSliceProcessor requires an AudioBuffer");
        return;
    }
    m_frames_per_block = static_cast<uint32_t>(audio->get_num_samples());
}

void StreamSliceProcessor::on_detach(const std::shared_ptr<Buffer>&)
{
    for (size_t i = 0; i < m_slots.size(); ++i)
        detach_slot(i);
}

bool StreamSliceProcessor::is_compatible_with(const std::shared_ptr<Buffer>& buffer) const
{
    return std::dynamic_pointer_cast<AudioBuffer>(buffer) != nullptr;
}

void StreamSliceProcessor::detach_slot(size_t index)
{
    auto& slot = m_slots[index];
    if (slot.proc && slot.slice.stream)
        slot.proc->on_detach(slot.slice.stream);
    slot = {};
}

void StreamSliceProcessor::load(size_t index, Kakshya::StreamSlice slice)
{
    if (m_frames_per_block == 0) {
        MF_ERROR(Journal::Component::Buffers, Journal::Context::BufferProcessing,
            "StreamSliceProcessor::load called before attachment to a buffer -- attach first");
        return;
    }

    if (!slice.stream)
        return;

    if (index > m_slots.size()) {
        MF_WARN(Journal::Component::Buffers, Journal::Context::Configuration,
            "StreamSliceProcessor::load index {} exceeds slot count {}; use {} for next available",
            index, m_slots.size(), m_slots.size());
    }

    if (index >= m_slots.size())
        m_slots.resize(index + 1);

    detach_slot(index);

    slice.active = false;
    slice.index = static_cast<uint8_t>(index);

    auto proc = std::make_shared<Kakshya::CursorAccessProcessor>(m_frames_per_block);
    proc->set_loop_region(slice.start_frame(), slice.end_frame());
    proc->set_looping(slice.looping);
    proc->set_loop_count(slice.loop_count);

    if (slice.speed != 1.0)
        proc->set_speed(slice.speed);

    proc->set_on_end([this, index] {
        m_slots[index].slice.active = false;
        if (m_on_end)
            m_on_end(index);
    });

    proc->on_attach(slice.stream);

    m_slots[index].slice = std::move(slice);
    m_slots[index].proc = std::move(proc);
}

void StreamSliceProcessor::bind(size_t index)
{
    if (index >= m_slots.size() || !m_slots[index].proc)
        return;

    m_slots[index].proc->reset();
    m_slots[index].slice.active = true;
}

void StreamSliceProcessor::unbind(size_t index)
{
    if (index >= m_slots.size() || !m_slots[index].proc)
        return;

    m_slots[index].proc->stop();
    m_slots[index].slice.active = false;
}

bool StreamSliceProcessor::any_active() const
{
    return std::ranges::any_of(m_slots, [](const auto& slot) {
        return slot.slice.active;
    });
}

void StreamSliceProcessor::processing_function(const std::shared_ptr<Buffer>& buffer)
{
    auto audio = std::dynamic_pointer_cast<AudioBuffer>(buffer);
    if (!audio)
        return;

    const uint32_t ch = audio->get_channel_id();
    auto& dst = audio->get_data();
    std::ranges::fill(dst, 0.0);

    for (auto& slot : m_slots) {
        if (!slot.slice.active || !slot.slice.stream || !slot.proc)
            continue;

        slot.proc->process(slot.slice.stream);

        const auto& pd = slot.slice.stream->get_dynamic_data(slot.proc->get_slot_index());
        if (pd.empty())
            continue;

        const auto structure = slot.slice.stream->get_structure();
        std::vector<double> tmp(dst.size(), 0.0);
        Kakshya::extract_processed_data(pd, structure.organization, structure.get_channel_count(), ch, tmp);

        for (size_t s = 0; s < dst.size(); ++s)
            dst[s] += tmp[s];
    }
}

} // namespace MayaFlux::Buffers
