#include "StreamSliceProcessor.hpp"

#include "MayaFlux/Buffers/AudioBuffer.hpp"
#include "MayaFlux/Kakshya/Utils/ContainerUtils.hpp"

#include "MayaFlux/Journal/Archivist.hpp"

namespace MayaFlux::Buffers {

namespace {

    void apply_slice(const Kakshya::StreamSlice& slice, Kakshya::CursorAccessProcessor& proc)
    {
        proc.set_loop_region(slice.start_frame(), slice.end_frame() + 1);
        proc.set_looping(slice.looping);
        proc.set_speed(slice.speed);
        proc.set_time_map(slice.time_map);
        proc.set_repeat(slice.repeat);

        if (slice.loop_count != proc.loop_count())
            proc.set_loop_count(slice.loop_count);
    }

}

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
    apply_slice(slice, *proc);

    proc->set_on_end([this, index] {
        m_slots[index].slice.active = false;
        if (m_on_end)
            m_on_end(index);
    });

    proc->on_attach(slice.stream);

    m_slots[index].slice = std::move(slice);
    m_slots[index].proc = std::move(proc);
}

void StreamSliceProcessor::bind(size_t index, uint32_t offset)
{
    if (index >= m_slots.size() || !m_slots[index].proc)
        return;

    apply_slice(m_slots[index].slice, *m_slots[index].proc);
    m_slots[index].proc->reset();
    m_slots[index].played = 0;
    m_slots[index].offset = offset;
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

        if (slot.offset >= dst.size()) {
            slot.offset -= static_cast<uint32_t>(dst.size());
            continue;
        }

        const size_t offset = slot.offset;
        const size_t frames = dst.size() - offset;

        if (offset > 0)
            slot.proc->set_frames_per_block(frames);

        apply_slice(slot.slice, *slot.proc);
        slot.proc->process(slot.slice.stream);

        if (offset > 0) {
            slot.proc->set_frames_per_block(m_frames_per_block);
            slot.offset = 0;
        }

        const uint64_t first = slot.played;
        slot.played += frames;

        const auto& pd = slot.slice.stream->get_dynamic_data(slot.proc->get_slot_index());
        if (pd.empty())
            continue;

        const auto structure = slot.slice.stream->get_structure();
        std::vector<double> tmp(frames, 0.0);
        Kakshya::extract_processed_data(pd, structure.organization, structure.get_channel_count(),
            slot.slice.source_channel.value_or(ch), tmp);

        const auto& envelope = slot.slice.envelope;
        if (envelope && envelope->fn) {
            const auto rate = static_cast<double>(slot.slice.stream->get_sample_rate());
            for (size_t s = 0; s < frames; ++s)
                dst[offset + s] += tmp[s] * slot.slice.scale * (*envelope)(static_cast<double>(first + s) / rate);
            continue;
        }

        for (size_t s = 0; s < frames; ++s)
            dst[offset + s] += tmp[s] * slot.slice.scale;
    }
}

} // namespace MayaFlux::Buffers
